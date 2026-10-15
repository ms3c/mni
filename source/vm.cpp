#include "vm.h"

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <stdexcept>
#include <string>

#include "checked_arithmetic.h"
#include "compiler.h"
#include "stack.h"

namespace mni {

  namespace {
    // Native code that calls back into Nuru recurses on the C++ stack; keep this much free.
    constexpr size_t kMinStackBytes = 256 * 1024;

    constexpr const char* kRecursion = "Kazi zimejiita mara nyingi mno (recursion imezidi kikomo)";

    // The name `@` is bound to inside a pakeji.
    Symbol atSymbol() {
      static const Symbol symbol = intern("@");
      return symbol;
    }

    size_t slot(int32_t operand) { return static_cast<size_t>(operand); }
  }  // namespace

  Value Vm::runProgram(const Program& program) {
    return runTopLevel(compileProgram(program, Proto::Kind::Main), in_.globals_.get(), nullptr);
  }

  void Vm::runModule(const Program& program, const std::shared_ptr<Environment>& scope) {
    runTopLevel(compileProgram(program, Proto::Kind::Module), scope.get(), scope);
  }

  Value Vm::runTopLevel(std::unique_ptr<Proto> proto, Environment* scope,
                        const std::shared_ptr<Environment>& owner) {
    protos_.push_back(std::move(proto));
    const Proto* top = protos_.back().get();
#ifndef NDEBUG
    if (std::string problem = verify(*top); !problem.empty())
      throw std::logic_error("bytecode verification failed: " + problem);
#endif

    Frame frame;
    frame.proto = top;
    frame.base = stack_.size();
    frame.localBase = locals_.size();
    frame.env = scope;
    frame.owner = owner;
    frame.depthBefore = in_.callDepth_;
    frame.native = true;
    const size_t needed = stack_.size() + static_cast<size_t>(top->maxStack);
    if (stack_.capacity() < needed) stack_.reserve(std::max(needed, stack_.capacity() * 2));
    frames_.push_back(std::move(frame));
    return execute();
  }

  Value Vm::callFunction(const Value& callee, std::vector<Value> args, int line, NamedArgs named) {
    const size_t frameCount = frames_.size();
    const size_t stackHeight = stack_.size();
    const int callDepth = in_.callDepth_;
    try {
      callWithNamed(callee, args, named, line, true);
      return execute();
    } catch (...) {
      unwindTo(frameCount, stackHeight, callDepth);
      throw;
    }
  }

  void Vm::truncateLocals(size_t size) {
    locals_.resize(size);
    localSet_.resize(size);
  }

  void Vm::reset() {
    frames_.clear();
    locals_.clear();
    localSet_.clear();
    stack_.clear();
  }

  void Vm::unwindTo(size_t frameCount, size_t stackHeight, int callDepth) {
    if (frames_.size() > frameCount) {
      truncateLocals(frames_[frameCount].localBase);
      frames_.erase(frames_.begin() + static_cast<std::ptrdiff_t>(frameCount), frames_.end());
    }
    if (stack_.size() > stackHeight) stack_.resize(stackHeight);
    in_.callDepth_ = callDepth;
  }

  namespace {
    bool intCompare(TokenType op, int64_t a, int64_t b) {
      switch (op) {
        case TokenType::Lt:
          return a < b;
        case TokenType::Lte:
          return a <= b;
        case TokenType::Gt:
          return a > b;
        case TokenType::Gte:
          return a >= b;
        case TokenType::Eq:
          return a == b;
        default:  // NotEq
          return a != b;
      }
    }

    // Integer arithmetic and comparisons that cannot fail, done in place on the left operand.
    // False means the generic path has to handle it (overflow, other operators).
    bool intBinary(TokenType op, Value& lhs, int64_t b) {
      const int64_t a = lhs.asInt();
      int64_t out = 0;
      switch (op) {
        case TokenType::Plus:
          if (addOverflows(a, b, out)) return false;
          lhs = Value::integer(out);
          return true;
        case TokenType::Minus:
          if (subOverflows(a, b, out)) return false;
          lhs = Value::integer(out);
          return true;
        case TokenType::Lt:
        case TokenType::Lte:
        case TokenType::Gt:
        case TokenType::Gte:
        case TokenType::Eq:
        case TokenType::NotEq:
          lhs = Value::boolean(intCompare(op, a, b));
          return true;
        default:
          return false;
      }
    }
  }  // namespace

  // ---- calls ----

  // Starts a call whose callee and arguments sit on the operand stack. Only the positional form
  // (the common case) goes through here.
  void Vm::callFromStack(const Value& callee, size_t calleeIndex, size_t argc, int line) {
    const Function& fn = callee.asFunction();
    const Proto* proto = fn.proto;
    const size_t paramCount = proto->params.size();
    if (argc > paramCount)
      throw NuruError(line, "Kazi inapokea hoja " + std::to_string(paramCount)
                                + " tu, lakini umetoa " + std::to_string(argc));
    if (proto->slotted) {
      // no Environment: the arguments go straight into the function's slots
      const size_t mark = locals_.size();
      std::shared_ptr<Environment> closure = fn.closure;
      checkCallLimits(line, false);
      locals_.resize(mark + proto->locals.size());
      localSet_.resize(mark + proto->locals.size());
      for (size_t i = 0; i < argc; ++i) {
        const size_t at = mark + static_cast<size_t>(proto->paramSlots[i]);
        locals_[at] = std::move(stack_[calleeIndex + 1 + i]);
        localSet_[at] = 1;
      }
      stack_.resize(calleeIndex);
      pushFunctionFrame(proto, closure, argc, line, false);
      return;
    }
    auto scope = Environment::make(fn.closure, paramCount + 4);
    for (size_t i = 0; i < argc; ++i)
      scope->define(proto->params[i].symbol, std::move(stack_[calleeIndex + 1 + i]));
    stack_.resize(calleeIndex);  // the callee and its arguments are gone; the result goes here
    checkCallLimits(line, false);
    pushFunctionFrame(proto, scope, argc, line, false);
  }

  // The general form: keyword arguments, and calls that come from native code.
  void Vm::callWithNamed(const Value& callee, std::vector<Value>& args, NamedArgs& named, int line,
                         bool native) {
    const Function& fn = callee.asFunction();
    const Proto* proto = fn.proto;
    const size_t paramCount = proto->params.size();
    if (args.size() > paramCount)
      throw NuruError(line, "Kazi inapokea hoja " + std::to_string(paramCount)
                                + " tu, lakini umetoa " + std::to_string(args.size()));

    if (proto->slotted) {
      const size_t mark = locals_.size();
      std::shared_ptr<Environment> closure = fn.closure;
      locals_.resize(mark + proto->locals.size());
      localSet_.resize(mark + proto->locals.size());
      try {
        for (size_t i = 0; i < args.size(); ++i) {
          const size_t at = mark + static_cast<size_t>(proto->paramSlots[i]);
          locals_[at] = std::move(args[i]);
          localSet_[at] = 1;
        }
        for (auto& [name, value] : named) {
          size_t j = 0;
          while (j < paramCount && proto->params[j].name != name) ++j;
          if (j == paramCount) throw NuruError(line, "Kazi haina kigezo '" + name + "'");
          const size_t at = mark + static_cast<size_t>(proto->paramSlots[j]);
          if (localSet_[at])
            throw NuruError(line, "Kigezo '" + name + "' kimepewa thamani mara mbili");
          locals_[at] = std::move(value);
          localSet_[at] = 1;
        }
        checkCallLimits(line, native);
      } catch (...) {
        truncateLocals(mark);
        throw;
      }
      pushFunctionFrame(proto, closure, args.size() + named.size(), line, native);
      return;
    }

    // positional arguments first, then keyword arguments by parameter name
    auto scope = Environment::make(fn.closure, paramCount + 4);
    for (size_t i = 0; i < args.size(); ++i)
      scope->define(proto->params[i].symbol, std::move(args[i]));
    for (auto& [name, value] : named) {
      size_t j = 0;
      while (j < paramCount && proto->params[j].name != name) ++j;
      if (j == paramCount) throw NuruError(line, "Kazi haina kigezo '" + name + "'");
      const Symbol symbol = proto->params[j].symbol;
      if (scope->lookupOwn(symbol))
        throw NuruError(line, "Kigezo '" + name + "' kimepewa thamani mara mbili");
      scope->define(symbol, std::move(value));
    }
    checkCallLimits(line, native);
    pushFunctionFrame(proto, scope, args.size() + named.size(), line, native);
  }

  // The recursion limit and the tick, in the order a call meets them.
  void Vm::checkCallLimits(int line, bool native) {
    if (in_.callDepth_ >= in_.maxCallDepth_ || (native && stackBytesRemaining() < kMinStackBytes))
      throw NuruError(line, kRecursion);
    in_.tick(line);
  }

  void Vm::pushFunctionFrame(const Proto* proto, const std::shared_ptr<Environment>& scope,
                             size_t bound, int line, bool native) {
    // Grow in big steps: reserving the exact size every call would reallocate on every call.
    const size_t needed = stack_.size() + static_cast<size_t>(proto->maxStack);
    if (stack_.capacity() < needed) stack_.reserve(std::max(needed, stack_.capacity() * 2));

    Frame& frame = frames_.emplace_back();
    frame.proto = proto;
    frame.base = stack_.size();
    // a slotted function's slots were just added at the end of locals_
    frame.localBase = proto->slotted ? locals_.size() - proto->locals.size() : locals_.size();
    frame.env = scope.get();
    frame.owner = scope;
    frame.callLine = line;
    frame.bound = bound;
    frame.depthBefore = in_.callDepth_;
    frame.native = native;
    ++in_.callDepth_;
  }

  // object.method(positional..., name = value...): the stack holds the object, then the positional
  // arguments, then the keyword values.
  void Vm::callMethod(const Instr& instr, const Proto& proto, int line) {
    const CallInfo& info = proto.calls[slot(instr.b)];
    const size_t positional = static_cast<size_t>(info.positional);
    const size_t total = positional + info.named.size();
    const size_t selfIndex = stack_.size() - total - 1;

    Value self = stack_[selfIndex];
    std::vector<Value> args;
    args.reserve(positional);
    for (size_t i = 0; i < positional; ++i) args.push_back(std::move(stack_[selfIndex + 1 + i]));
    NamedArgs named;
    for (size_t i = 0; i < info.named.size(); ++i)
      named.emplace_back(info.named[i], std::move(stack_[selfIndex + 1 + positional + i]));
    stack_.resize(selfIndex);

    if (self.type() == Value::Type::Package) {
      // the member is looked up after the arguments were evaluated
      Value callee = in_.getProperty(self, info.method, line);
      if (callee.type() == Value::Type::Function) {
        callWithNamed(callee, args, named, line, false);
        return;
      }
      stack_.push_back(in_.callFunction(callee, std::move(args), line, std::move(named)));
      return;
    }
    stack_.push_back(in_.callMethod(self, info.name, args, named, line));
  }

  // ---- running ----

  Value Vm::execute() {
    const size_t frameCount = frames_.size() - 1;  // the frame the caller just pushed is the base
    const size_t stackHeight = frames_.back().base;
    const int callDepth = frames_.back().depthBefore;
    try {
      return dispatch();
    } catch (...) {
      unwindTo(frameCount, stackHeight, callDepth);
      throw;
    }
  }

  Value Vm::dispatch() {
    Frame* f = nullptr;
    const Instr* code = nullptr;
    const int* lines = nullptr;
    size_t pc = 0;

    // The line of the instruction being executed.
#define MNI_LINE() lines[pc - 1]

  reload:  // (re)load the running frame; needed whenever a call or return changes it
    f = &frames_.back();
    code = f->proto->code.data();
    lines = f->proto->lines.data();
    pc = f->pc;

    for (;;) {
      const Instr& ins = code[pc++];
      switch (ins.op) {
        case Op::Const:
          stack_.push_back(f->proto->consts[slot(ins.b)]);
          break;
        case Op::Null:
          stack_.push_back(Value());
          break;
        case Op::True:
          stack_.push_back(Value::boolean(true));
          break;
        case Op::False:
          stack_.push_back(Value::boolean(false));
          break;
        case Op::Pop:
          stack_.pop_back();
          break;
        case Op::PopN:
          stack_.resize(stack_.size() - slot(ins.b));
          break;
        case Op::Dup: {
          Value copy = stack_.back();
          stack_.push_back(std::move(copy));
          break;
        }
        case Op::Dup2: {
          Value first = stack_[stack_.size() - 2];
          Value second = stack_[stack_.size() - 1];
          stack_.push_back(std::move(first));
          stack_.push_back(std::move(second));
          break;
        }

        case Op::GetVar: {
          const Symbol symbol = static_cast<Symbol>(ins.b);
          Value* value = f->env->lookup(symbol);
          if (!value)
            throw NuruError(MNI_LINE(), "Kitambulishi hakipatikani: " + symbolName(symbol));
          stack_.push_back(*value);
          break;
        }
        case Op::GetSlot: {
          const size_t at = f->localBase + slot(ins.b);
          if (localSet_[at]) {
            stack_.push_back(locals_[at]);
            break;
          }
          const Symbol symbol = f->proto->locals[slot(ins.b)];
          Value* value = f->env->lookup(symbol);
          if (!value)
            throw NuruError(MNI_LINE(), "Kitambulishi hakipatikani: " + symbolName(symbol));
          stack_.push_back(*value);
          break;
        }
        case Op::DefineSlot: {
          const size_t at = f->localBase + slot(ins.b);
          locals_[at] = pop();
          localSet_[at] = 1;
          break;
        }
        case Op::DefineSlotKeep: {
          const size_t at = f->localBase + slot(ins.b);
          locals_[at] = stack_.back();
          localSet_[at] = 1;
          break;
        }
        case Op::AssignSlot: {
          const size_t at = f->localBase + slot(ins.b);
          if (!localSet_[at]) {
            // not yet a local: an existing outer variable wins, as Environment::assign does
            Value copy = stack_.back();
            if (!f->env->assignOuter(f->proto->locals[slot(ins.b)], copy)) {
              locals_[at] = stack_.back();
              localSet_[at] = 1;
            }
          } else {
            locals_[at] = stack_.back();
          }
          break;
        }
        case Op::JumpIfSlotSet:
          if (localSet_[f->localBase + slot(ins.a)]) pc = slot(ins.b);
          break;
        case Op::DefineVar: {
          Value value = pop();
          f->env->define(static_cast<Symbol>(ins.b), std::move(value));
          break;
        }
        case Op::DefineVarKeep:
          f->env->define(static_cast<Symbol>(ins.b), stack_.back());
          break;
        case Op::AssignVar:
          f->env->assign(static_cast<Symbol>(ins.b), stack_.back());
          break;
        case Op::LoadAt: {
          Value* self = f->env->lookup(atSymbol());
          if (!self) throw NuruError(MNI_LINE(), "'@' inatumika ndani ya pakeji tu");
          stack_.push_back(*self);
          break;
        }

        case Op::BinOp: {
          // Integer operands that cannot fail are handled in place, without the generic call.
          Value& lhs = stack_[stack_.size() - 2];
          const Value& rhs = stack_.back();
          if (lhs.type() == Value::Type::Int && rhs.type() == Value::Type::Int
              && intBinary(static_cast<TokenType>(ins.a), lhs, rhs.asInt())) {
            stack_.pop_back();
            break;
          }
          Value right = pop();
          Value left = pop();
          stack_.push_back(in_.binaryOp(static_cast<TokenType>(ins.a), left, right, MNI_LINE()));
          break;
        }
        case Op::BinOpK: {
          const Value& rhs = f->proto->consts[slot(ins.b)];
          Value& lhs = stack_.back();
          if (lhs.type() == Value::Type::Int && rhs.type() == Value::Type::Int
              && intBinary(static_cast<TokenType>(ins.a), lhs, rhs.asInt()))
            break;
          Value left = pop();
          stack_.push_back(in_.binaryOp(static_cast<TokenType>(ins.a), left, rhs, MNI_LINE()));
          break;
        }
        case Op::JumpIfNotCmp: {
          const Value& lhs = stack_[stack_.size() - 2];
          const Value& rhs = stack_.back();
          bool result = false;
          if (lhs.type() == Value::Type::Int && rhs.type() == Value::Type::Int) {
            result = intCompare(static_cast<TokenType>(ins.a), lhs.asInt(), rhs.asInt());
          } else {
            result = in_.binaryOp(static_cast<TokenType>(ins.a), lhs, rhs, MNI_LINE()).isTruthy();
          }
          stack_.pop_back();
          stack_.pop_back();
          if (!result) pc = slot(ins.b);
          break;
        }
        case Op::JumpIfNotCmpK: {
          const auto token = static_cast<TokenType>(ins.a & 0xff);
          const Value& rhs = f->proto->consts[slot(ins.a >> 8)];
          const Value& lhs = stack_.back();
          bool result = false;
          if (lhs.type() == Value::Type::Int && rhs.type() == Value::Type::Int) {
            result = intCompare(token, lhs.asInt(), rhs.asInt());
          } else {
            result = in_.binaryOp(token, lhs, rhs, MNI_LINE()).isTruthy();
          }
          stack_.pop_back();
          if (!result) pc = slot(ins.b);
          break;
        }
        case Op::Prefix: {
          Value right = pop();
          const auto op = static_cast<TokenType>(ins.a);
          bool ok = true;
          Value result;
          switch (op) {
            case TokenType::Bang:
              result = Value::boolean(!right.isTruthy());
              break;
            case TokenType::Minus:
              if (right.type() == Value::Type::Int) {
                result = Value::integer(-right.asInt());
              } else if (right.type() == Value::Type::Float) {
                result = Value::real(-right.asFloat());
              } else {
                ok = false;
              }
              break;
            case TokenType::Plus:
              if (right.isNumber()) {
                result = right;
              } else {
                ok = false;
              }
              break;
            default:
              ok = false;
              break;
          }
          if (!ok)
            throw NuruError(MNI_LINE(), "Operesheni haijulikani: " + std::string(tokenTypeName(op))
                                            + right.typeName());
          stack_.push_back(std::move(result));
          break;
        }
        case Op::Incr:
        case Op::Decr: {
          const bool up = ins.op == Op::Incr;
          Value& top = stack_.back();
          if (top.type() == Value::Type::Int) {
            top = Value::integer(top.asInt() + (up ? 1 : -1));
          } else if (top.type() == Value::Type::Float) {
            top = Value::real(top.asFloat() + (up ? 1.0 : -1.0));
          } else {
            throw NuruError(MNI_LINE(), std::string("'") + (up ? "++" : "--")
                                            + "' inafanya kazi kwa namba tu, si " + top.typeName());
          }
          break;
        }
        case Op::Truthy: {
          Value& top = stack_.back();
          top = Value::boolean(top.isTruthy());
          break;
        }

        case Op::GetIndex: {
          Value indexValue = pop();
          Value object = pop();
          stack_.push_back(in_.indexValue(object, indexValue, MNI_LINE()));
          break;
        }
        case Op::SetIndex: {
          Value value = pop();
          Value indexValue = pop();
          Value object = pop();
          in_.setIndex(object, indexValue, value, MNI_LINE());
          stack_.push_back(std::move(value));
          break;
        }
        case Op::GetProp: {
          Value object = pop();
          stack_.push_back(in_.getProperty(object, static_cast<Symbol>(ins.b), MNI_LINE()));
          break;
        }
        case Op::SetProp: {
          Value value = pop();
          Value object = pop();
          in_.setProperty(object, static_cast<Symbol>(ins.b), value, MNI_LINE());
          stack_.push_back(std::move(value));
          break;
        }

        case Op::MakeArray: {
          const size_t count = slot(ins.b);
          std::vector<Value> items(
              std::make_move_iterator(stack_.end() - static_cast<std::ptrdiff_t>(count)),
              std::make_move_iterator(stack_.end()));
          stack_.resize(stack_.size() - count);
          stack_.push_back(Value::array(std::move(items)));
          break;
        }
        case Op::MakeDict:
          stack_.push_back(Value::dict());
          break;
        case Op::DictSet: {
          Value value = pop();
          Value key = pop();
          if (!Dict::hashable(key))
            throw NuruError(MNI_LINE(),
                            std::string("Ufunguo wa kamusi hauwezi kuwa ") + key.typeName());
          stack_.back().asDict().set(key, std::move(value));
          break;
        }
        case Op::MakeFunction: {
          const Proto* child = f->proto->children[slot(ins.b)].get();
          stack_.push_back(Value::function(std::make_shared<Function>(
              Function{child->node, f->env->shared_from_this(), child})));
          break;
        }

        case Op::Jump:
          if (slot(ins.b) < pc) in_.tick(MNI_LINE());  // a backward jump is another loop iteration
          pc = slot(ins.b);
          break;
        case Op::JumpIfFalse: {
          const bool truthy = stack_.back().isTruthy();
          stack_.pop_back();
          if (!truthy) pc = slot(ins.b);
        } break;
        case Op::JumpIfTrue:
          if (pop().isTruthy()) pc = slot(ins.b);
          break;
        case Op::JumpIfOwnDefined:
          if (f->env->lookupOwn(static_cast<Symbol>(ins.a))) pc = slot(ins.b);
          break;
        case Op::DefaultsBegin:
          if (f->bound >= f->proto->params.size()) pc = slot(ins.b);
          break;

        case Op::Call: {
          f->pc = pc;
          const size_t argc = slot(ins.b);
          const size_t calleeIndex = stack_.size() - argc - 1;
          if (stack_[calleeIndex].type() == Value::Type::Function) {
            // callFromStack is done with the callee before it clears the stack
            callFromStack(stack_[calleeIndex], calleeIndex, argc, MNI_LINE());
          } else {
            Value callee = stack_[calleeIndex];
            std::vector<Value> args(
                std::make_move_iterator(stack_.begin() + static_cast<std::ptrdiff_t>(calleeIndex)
                                        + 1),
                std::make_move_iterator(stack_.end()));
            stack_.resize(calleeIndex);
            stack_.push_back(in_.callFunction(callee, std::move(args), MNI_LINE()));
          }
          goto reload;
        }
        case Op::CallMethod:
          f->pc = pc;
          callMethod(ins, *f->proto, MNI_LINE());
          goto reload;
        case Op::Return: {
          Value result = pop();
          Frame& done = frames_.back();
          stack_.resize(done.base);
          if (locals_.size() > done.localBase) truncateLocals(done.localBase);
          in_.callDepth_ = done.depthBefore;
          if (done.proto->kind == Proto::Kind::Package) {
            done.parentEnv->define(done.packageName, done.package);
            result = std::move(done.package);
          }
          const bool native = done.native;
          frames_.pop_back();
          if (native) return result;
          stack_.push_back(std::move(result));
          goto reload;
        }

        case Op::IterInit: {
          Value iterable = pop();
          std::vector<Value> items;
          int64_t stride = 1;
          switch (iterable.type()) {
            case Value::Type::Array:
              items = iterable.asArray().items;
              break;
            case Value::Type::String:
              for (std::string& character : utf8Chars(iterable.asString()))
                items.push_back(Value::string(std::move(character)));
              break;
            case Value::Type::Dict:
              stride = 2;  // keys and values alternate
              for (const auto& [key, value] : iterable.asDict().entries) {
                items.push_back(key);
                items.push_back(value);
              }
              break;
            default:
              throw NuruError(MNI_LINE(),
                              std::string("Huwezi kuzunguka juu ya ") + iterable.typeName());
          }
          stack_.push_back(Value::array(std::move(items)));  // a private copy to iterate over
          stack_.push_back(Value::integer(0));
          stack_.push_back(Value::integer(stride));
          break;
        }
        case Op::IterNext: {
          const size_t top = stack_.size();
          const int64_t cursor = stack_[top - 2].asInt();
          const int64_t stride = stack_[top - 1].asInt();
          const std::vector<Value>& items = stack_[top - 3].asArray().items;
          const size_t at = static_cast<size_t>(cursor);
          if (at >= items.size()) {
            pc = slot(ins.b);
            break;
          }
          const ForInInfo& info = f->proto->forIns[slot(ins.a)];
          Value key = stride == 1 ? Value::integer(cursor) : items[at];
          Value value = stride == 1 ? items[at] : items[at + 1];
          if (f->proto->slotted) {
            if (info.hasKey) {
              locals_[f->localBase + slot(info.keySlot)] = std::move(key);
              localSet_[f->localBase + slot(info.keySlot)] = 1;
            }
            locals_[f->localBase + slot(info.valueSlot)] = std::move(value);
            localSet_[f->localBase + slot(info.valueSlot)] = 1;
          } else {
            if (info.hasKey) f->env->define(info.key, std::move(key));
            f->env->define(info.value, std::move(value));
          }
          stack_[top - 2] = Value::integer(cursor + stride);
          break;
        }
        case Op::IterPop:
          stack_.resize(stack_.size() - 3);
          break;

        case Op::JumpIfSwitchEq: {
          Value candidate = pop();
          if (valuesEqual(stack_.back(), candidate)) pc = slot(ins.b);
          break;
        }

        case Op::MakePackage: {
          f->pc = pc;
          const Proto* body = f->proto->children[slot(ins.b)].get();
          auto package = std::make_shared<Package>();
          package->name = body->name;
          package->typeName = "PAKEJI";
          package->env = std::make_shared<Environment>(f->env->shared_from_this());
          package->env->setBoundary();
          Value value = Value::package(package);
          package->env->define(atSymbol(), value);  // `@` inside the pakeji

          Frame frame;
          frame.proto = body;
          frame.base = stack_.size();
          frame.env = package->env.get();
          frame.owner = package->env;
          frame.callLine = MNI_LINE();
          frame.depthBefore = in_.callDepth_;
          frame.package = std::move(value);
          frame.parentEnv = f->env;  // the pakeji's name is defined here when its body finishes
          frame.packageName = intern(body->name);
          const size_t needed = stack_.size() + static_cast<size_t>(body->maxStack);
          if (stack_.capacity() < needed) stack_.reserve(std::max(needed, stack_.capacity() * 2));
          frames_.push_back(std::move(frame));
          goto reload;
        }
        case Op::Import: {
          f->pc = pc;
          const std::string& name = f->proto->consts[slot(ins.b)].asString();
          Environment* scope = f->env;  // loading may run other code, which moves the frames
          Value loaded = in_.loadModule(name, MNI_LINE());
          scope->define(std::filesystem::path(name).stem().string(), std::move(loaded));
          goto reload;
        }

        case Op::Throw:
          throw NuruError(ins.a ? f->callLine : MNI_LINE(),
                          f->proto->consts[slot(ins.b)].asString());
      }
    }
#undef MNI_LINE
  }

}  // namespace mni
