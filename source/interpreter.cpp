#include <mni/interpreter.h>
#include <mni/parser.h>

#include <cmath>

#include "stack.h"

namespace mni {

  namespace {
    // Evaluates `expr` into `var`; if that raised a pending break/continue/return, bail out.
#define NURU_EVAL(var, expr) \
  Value var = eval(expr);    \
  if (flow_ != Flow::None) return Value();

    std::string joinLines(const std::vector<std::string>& lines) {
      std::string out;
      for (size_t i = 0; i < lines.size(); ++i) out += (i ? "\n" : "") + lines[i];
      return out;
    }

    // Stack that must remain free when calling a function. Between two calls the evaluator uses
    // a few KB (more in debug builds), so this leaves room for the code that runs in between.
    constexpr std::size_t kMinStackBytes = 256 * 1024;

    TokenType baseOperator(TokenType assignOp) {
      switch (assignOp) {
        case TokenType::PlusAssign:
          return TokenType::Plus;
        case TokenType::MinusAssign:
          return TokenType::Minus;
        case TokenType::AsteriskAssign:
          return TokenType::Asterisk;
        case TokenType::SlashAssign:
          return TokenType::Slash;
        default:
          return TokenType::Modulus;
      }
    }
  }  // namespace

  SyntaxError::SyntaxError(std::vector<std::string> errs)
      : std::runtime_error(joinLines(errs)), errors(std::move(errs)) {}

  Interpreter::Interpreter(std::ostream& out, std::istream& in)
      : out_(out), in_(in), globals_(std::make_shared<Environment>()), env_(globals_) {
    installBuiltins();
  }

  Interpreter::~Interpreter() = default;

  Value Interpreter::run(const std::string& source) {
    Parser parser(source);
    auto program = std::make_unique<Program>(parser.parseProgram());
    if (!parser.errors().empty()) throw SyntaxError(parser.errors());
    programs_.push_back(std::move(program));

    try {
      return execProgram(*programs_.back());
    } catch (...) {
      flow_ = Flow::None;
      env_ = globals_;
      callDepth_ = 0;
      throw;
    }
  }

  // Runs the statements in the current scope. Returns the last statement's value.
  Value Interpreter::execProgram(const Program& program) {
    Value last;
    for (const auto& stmt : program.statements) {
      last = exec(stmt.get());
      if (flow_ == Flow::Return) {  // a top-level `rudisha` ends the script
        last = std::move(returnValue_);
        break;
      }
      if (flow_ != Flow::None)
        throw NuruError(stmt->token.line, "'vunja' na 'endelea' lazima ziwe ndani ya kitanzi");
    }
    flow_ = Flow::None;
    returnValue_ = Value();
    return last;
  }

  Value Interpreter::exec(const Statement* stmt) {
    switch (stmt->kind) {
      case NodeKind::Let: {
        auto* s = static_cast<const LetStatement*>(stmt);
        NURU_EVAL(value, s->value.get())
        env_->define(s->name, std::move(value));
        return Value();
      }
      case NodeKind::Return: {
        auto* s = static_cast<const ReturnStatement*>(stmt);
        Value value;
        if (s->value) {
          value = eval(s->value.get());
          if (flow_ != Flow::None) return Value();
        }
        returnValue_ = std::move(value);
        flow_ = Flow::Return;
        return Value();
      }
      case NodeKind::ExpressionStatement:
        return eval(static_cast<const ExpressionStatement*>(stmt)->expression.get());
      case NodeKind::Block:
        return execBlock(static_cast<const BlockStatement*>(stmt));
      case NodeKind::Break:
        flow_ = Flow::Break;
        return Value();
      case NodeKind::Continue:
        flow_ = Flow::Continue;
        return Value();
      default:
        throw NuruError(stmt->token.line, "Kauli isiyojulikana: " + stmt->toString());
    }
  }

  Value Interpreter::execBlock(const BlockStatement* block) {
    Value last;
    for (const auto& stmt : block->statements) {
      last = exec(stmt.get());
      if (flow_ != Flow::None) break;
    }
    return last;
  }

  Value Interpreter::eval(const Expression* expr) {
    switch (expr->kind) {
      case NodeKind::Identifier:
        return evalIdentifier(static_cast<const Identifier*>(expr));
      case NodeKind::IntegerLiteral:
        return Value::integer(static_cast<const IntegerLiteral*>(expr)->value);
      case NodeKind::FloatLiteral:
        return Value::real(static_cast<const FloatLiteral*>(expr)->value);
      case NodeKind::StringLiteral:
        return Value::string(static_cast<const StringLiteral*>(expr)->value);
      case NodeKind::BooleanLiteral:
        return Value::boolean(static_cast<const BooleanLiteral*>(expr)->value);
      case NodeKind::NullLiteral:
        return Value();
      case NodeKind::ArrayLiteral:
        return evalArray(static_cast<const ArrayLiteral*>(expr));
      case NodeKind::DictLiteral:
        return evalDict(static_cast<const DictLiteral*>(expr));
      case NodeKind::Prefix:
        return evalPrefix(static_cast<const PrefixExpression*>(expr));
      case NodeKind::Infix:
        return evalInfix(static_cast<const InfixExpression*>(expr));
      case NodeKind::Postfix:
        return evalPostfix(static_cast<const PostfixExpression*>(expr));
      case NodeKind::Assign:
        return evalAssign(static_cast<const AssignExpression*>(expr));
      case NodeKind::If:
        return evalIf(static_cast<const IfExpression*>(expr));
      case NodeKind::While:
        return evalWhile(static_cast<const WhileExpression*>(expr));
      case NodeKind::ForIn:
        return evalForIn(static_cast<const ForInExpression*>(expr));
      case NodeKind::Switch:
        return evalSwitch(static_cast<const SwitchExpression*>(expr));
      case NodeKind::Function:
        return evalFunction(static_cast<const FunctionLiteral*>(expr));
      case NodeKind::Call:
        return evalCall(static_cast<const CallExpression*>(expr));
      case NodeKind::MethodCall:
        return evalMethodCall(static_cast<const MethodCallExpression*>(expr));
      case NodeKind::Property:
        return evalProperty(static_cast<const PropertyExpression*>(expr));
      case NodeKind::Import:
        return evalImport(static_cast<const ImportExpression*>(expr));
      case NodeKind::Package:
        return evalPackage(static_cast<const PackageExpression*>(expr));
      case NodeKind::At:
        return evalAt(static_cast<const AtExpression*>(expr));
      case NodeKind::Index: {
        auto* e = static_cast<const IndexExpression*>(expr);
        NURU_EVAL(object, e->left.get())
        NURU_EVAL(index, e->index.get())
        return indexValue(object, index, e->token.line);
      }
      default:
        throw NuruError(expr->token.line, "Bado haijaandikwa: " + expr->toString());
    }
  }

  Value Interpreter::evalIdentifier(const Identifier* id) {
    if (Value* v = env_->lookup(id->name)) return *v;
    throw NuruError(id->token.line, "Kitambulishi hakipatikani: " + id->name);
  }

  Value Interpreter::evalArray(const ArrayLiteral* e) {
    std::vector<Value> items;
    items.reserve(e->elements.size());
    for (const auto& el : e->elements) {
      NURU_EVAL(v, el.get())
      items.push_back(std::move(v));
    }
    return Value::array(std::move(items));
  }

  Value Interpreter::evalDict(const DictLiteral* e) {
    Value dict = Value::dict();
    for (const auto& [keyExpr, valueExpr] : e->pairs) {
      NURU_EVAL(key, keyExpr.get())
      NURU_EVAL(value, valueExpr.get())
      if (!Dict::hashable(key))
        throw NuruError(keyExpr->token.line,
                        std::string("Ufunguo wa kamusi hauwezi kuwa ") + key.typeName());
      dict.asDict().set(key, std::move(value));
    }
    return dict;
  }

  Value Interpreter::evalPrefix(const PrefixExpression* e) {
    NURU_EVAL(right, e->right.get())
    const int line = e->token.line;
    switch (e->token.type) {
      case TokenType::Bang:
        return Value::boolean(!right.isTruthy());
      case TokenType::Minus:
        if (right.type() == Value::Type::Int) return Value::integer(-right.asInt());
        if (right.type() == Value::Type::Float) return Value::real(-right.asFloat());
        break;
      case TokenType::Plus:
        if (right.isNumber()) return right;
        break;
      default:
        break;
    }
    throw NuruError(line, "Operesheni haijulikani: " + e->op + std::string(right.typeName()));
  }

  Value Interpreter::evalInfix(const InfixExpression* e) {
    const TokenType op = e->token.type;
    if (op == TokenType::And || op == TokenType::Or) {
      NURU_EVAL(left, e->left.get())
      if ((op == TokenType::And) != left.isTruthy()) return Value::boolean(op == TokenType::Or);
      NURU_EVAL(right, e->right.get())
      return Value::boolean(right.isTruthy());
    }
    NURU_EVAL(left, e->left.get())
    NURU_EVAL(right, e->right.get())
    return binaryOp(op, left, right, e->token.line);
  }

  bool Interpreter::makeRef(const Expression* target, Ref& ref) {
    switch (target->kind) {
      case NodeKind::Identifier:
        ref.kind = Ref::Kind::Var;
        ref.name = static_cast<const Identifier*>(target)->name;
        return true;
      case NodeKind::Index: {
        auto* ix = static_cast<const IndexExpression*>(target);
        ref.kind = Ref::Kind::Index;
        ref.object = eval(ix->left.get());
        if (flow_ != Flow::None) return false;
        ref.index = eval(ix->index.get());
        return flow_ == Flow::None;
      }
      default: {  // Property: the parser only allows the three kinds
        auto* prop = static_cast<const PropertyExpression*>(target);
        ref.kind = Ref::Kind::Property;
        ref.name = prop->property;
        ref.object = eval(prop->object.get());
        return flow_ == Flow::None;
      }
    }
  }

  Value Interpreter::readRef(const Ref& ref, int line) {
    switch (ref.kind) {
      case Ref::Kind::Var: {
        Value* v = env_->lookup(ref.name);
        if (!v) throw NuruError(line, "Kitambulishi hakipatikani: " + ref.name);
        return *v;
      }
      case Ref::Kind::Index:
        return indexValue(ref.object, ref.index, line);
      default:
        return getProperty(ref.object, ref.name, line);
    }
  }

  void Interpreter::writeRef(const Ref& ref, const Value& value, int line) {
    switch (ref.kind) {
      case Ref::Kind::Var:
        env_->assign(ref.name, value);
        return;
      case Ref::Kind::Index:
        setIndex(ref.object, ref.index, value, line);
        return;
      default:
        setProperty(ref.object, ref.name, value, line);
        return;
    }
  }

  Value Interpreter::getProperty(const Value& object, const std::string& name, int line) {
    if (object.type() == Value::Type::Package) {
      const Package& pkg = object.asPackage();
      Value* member = name == "@" ? nullptr : pkg.env->lookupOwn(name);
      if (!member)
        throw NuruError(line,
                        "'" + name + "' haipo kwenye " + pkg.typeName + " '" + pkg.name + "'");
      return *member;
    }
    throw NuruError(line, "Sifa '" + name + "' haipatikani kwenye " + object.typeName());
  }

  void Interpreter::setProperty(const Value& object, const std::string& name, Value value,
                                int line) {
    if (object.type() == Value::Type::Package && name != "@") {
      object.asPackage().env->define(name, std::move(value));
      return;
    }
    throw NuruError(line, "Huwezi kuweka sifa '" + name + "' kwenye " + object.typeName());
  }

  Value Interpreter::evalAssign(const AssignExpression* e) {
    const int line = e->token.line;
    const bool compound = e->token.type != TokenType::Assign;
    Ref ref;
    if (!makeRef(e->target.get(), ref)) return Value();
    Value current;
    if (compound) current = readRef(ref, line);
    NURU_EVAL(value, e->value.get())
    if (compound) value = binaryOp(baseOperator(e->token.type), current, value, line);
    writeRef(ref, value, line);
    return value;
  }

  Value Interpreter::evalPostfix(const PostfixExpression* e) {
    const int line = e->token.line;
    const int64_t delta = e->token.type == TokenType::PlusPlus ? 1 : -1;
    Ref ref;
    if (!makeRef(e->operand.get(), ref)) return Value();
    const Value current = readRef(ref, line);
    Value updated;
    if (current.type() == Value::Type::Int) {
      updated = Value::integer(current.asInt() + delta);
    } else if (current.type() == Value::Type::Float) {
      updated = Value::real(current.asFloat() + static_cast<double>(delta));
    } else {
      throw NuruError(line, "'" + e->op + "' inafanya kazi kwa namba tu, si " + current.typeName());
    }
    writeRef(ref, updated, line);
    return updated;
  }

  Value Interpreter::evalIf(const IfExpression* e) {
    NURU_EVAL(condition, e->condition.get())
    if (condition.isTruthy()) return execBlock(e->consequence.get());
    if (e->alternative) return execBlock(e->alternative.get());
    return Value();
  }

  Value Interpreter::evalWhile(const WhileExpression* e) {
    for (;;) {
      NURU_EVAL(condition, e->condition.get())
      if (!condition.isTruthy()) return Value();
      execBlock(e->body.get());
      if (flow_ == Flow::Break) {
        flow_ = Flow::None;
        return Value();
      }
      if (flow_ == Flow::Continue) flow_ = Flow::None;
      if (flow_ != Flow::None) return Value();
    }
  }

  Value Interpreter::evalForIn(const ForInExpression* e) {
    NURU_EVAL(iterable, e->iterable.get())

    // Iterates over a copy, so the body may modify the container. False means stop looping.
    auto step = [&](Value key, Value value) {
      if (!e->key.empty()) env_->define(e->key, std::move(key));
      env_->define(e->value, std::move(value));
      execBlock(e->body.get());
      if (flow_ == Flow::Break) {
        flow_ = Flow::None;
        return false;
      }
      if (flow_ == Flow::Continue) flow_ = Flow::None;
      return flow_ == Flow::None;
    };

    switch (iterable.type()) {
      case Value::Type::Array: {
        const std::vector<Value> items = iterable.asArray().items;
        for (size_t i = 0; i < items.size(); ++i)
          if (!step(Value::integer(static_cast<int64_t>(i)), items[i])) break;
        return Value();
      }
      case Value::Type::String: {
        const std::vector<std::string> chars = utf8Chars(iterable.asString());
        for (size_t i = 0; i < chars.size(); ++i)
          if (!step(Value::integer(static_cast<int64_t>(i)), Value::string(chars[i]))) break;
        return Value();
      }
      case Value::Type::Dict: {
        const auto entries = iterable.asDict().entries;
        for (const auto& [k, v] : entries)
          if (!step(k, v)) break;
        return Value();
      }
      default:
        throw NuruError(e->token.line,
                        std::string("Huwezi kuzunguka juu ya ") + iterable.typeName());
    }
  }

  Value Interpreter::evalSwitch(const SwitchExpression* e) {
    NURU_EVAL(subject, e->value.get())
    const SwitchCase* fallback = nullptr;
    for (const SwitchCase& c : e->cases) {
      if (c.isDefault) {
        fallback = &c;
        continue;
      }
      for (const auto& valueExpr : c.values) {
        NURU_EVAL(candidate, valueExpr.get())
        if (valuesEqual(subject, candidate)) return execBlock(c.body.get());
      }
    }
    return fallback ? execBlock(fallback->body.get()) : Value();
  }

  Value Interpreter::evalFunction(const FunctionLiteral* e) {
    Value fn = Value::function(std::make_shared<Function>(Function{e, env_}));
    if (!e->name.empty()) env_->define(e->name, fn);
    return fn;
  }

  Value Interpreter::evalCall(const CallExpression* e) {
    NURU_EVAL(callee, e->callee.get())
    std::vector<Value> args;
    args.reserve(e->arguments.size());
    for (const auto& a : e->arguments) {
      NURU_EVAL(v, a.get())
      args.push_back(std::move(v));
    }
    return callFunction(callee, std::move(args), e->token.line);
  }

  Value Interpreter::evalMethodCall(const MethodCallExpression* e) {
    NURU_EVAL(self, e->object.get())
    std::vector<Value> args;
    args.reserve(e->arguments.size());
    for (const auto& a : e->arguments) {
      NURU_EVAL(v, a.get())
      args.push_back(std::move(v));
    }
    NamedArgs named;
    for (const auto& n : e->namedArguments) {
      NURU_EVAL(v, n.value.get())
      named.emplace_back(n.name, std::move(v));
    }
    return callMethod(self, e->method, args, named, e->token.line);
  }

  Value Interpreter::callFunction(const Value& callee, std::vector<Value> args, int line,
                                  NamedArgs named) {
    switch (callee.type()) {
      case Value::Type::Function:
        return callUser(callee, args, named, line);
      case Value::Type::Builtin: {
        Builtin& b = callee.asBuiltin();
        if (!named.empty() && !b.acceptsNamed)
          throw NuruError(line, "'" + b.name + "' haipokei hoja zenye majina");
        return b.fn(*this, args, named, line);
      }
      default:
        throw NuruError(line, std::string("Hii si kazi: ") + callee.typeName());
    }
  }

  Value Interpreter::callUser(const Value& callee, std::vector<Value>& args, NamedArgs& named,
                              int line) {
    const Function& fn = callee.asFunction();
    const FunctionLiteral* node = fn.node;
    const size_t paramCount = node->parameters.size();
    if (args.size() > paramCount)
      throw NuruError(line, "Kazi inapokea hoja " + std::to_string(paramCount)
                                + " tu, lakini umetoa " + std::to_string(args.size()));
    if (callDepth_ >= maxCallDepth_ || stackBytesRemaining() < kMinStackBytes)
      throw NuruError(line, "Kazi zimejiita mara nyingi mno (recursion imezidi kikomo)");

    // positional arguments first, then keyword arguments by parameter name
    std::vector<Value> values(paramCount);
    std::vector<bool> given(paramCount, false);
    for (size_t i = 0; i < args.size(); ++i) {
      values[i] = std::move(args[i]);
      given[i] = true;
    }
    for (auto& [name, value] : named) {
      size_t j = 0;
      while (j < paramCount && node->parameters[j].name != name) ++j;
      if (j == paramCount) throw NuruError(line, "Kazi haina kigezo '" + name + "'");
      if (given[j]) throw NuruError(line, "Kigezo '" + name + "' kimepewa thamani mara mbili");
      values[j] = std::move(value);
      given[j] = true;
    }

    struct Restore {
      std::shared_ptr<Environment>& env;
      std::shared_ptr<Environment> saved;
      int& depth;
      ~Restore() {
        env = std::move(saved);
        --depth;
      }
    };
    ++callDepth_;
    Restore restore{env_, env_, callDepth_};
    auto local = std::make_shared<Environment>(fn.closure);
    env_ = local;

    for (size_t i = 0; i < paramCount; ++i) {
      const Parameter& p = node->parameters[i];
      if (given[i]) {
        local->define(p.name, std::move(values[i]));
      } else if (p.defaultValue) {
        Value v = eval(p.defaultValue.get());
        local->define(p.name, std::move(v));
      } else {
        throw NuruError(line, "Kigezo '" + p.name + "' hakijapewa thamani");
      }
    }

    Value result = execBlock(node->body.get());  // value of the last statement unless `rudisha`
    if (flow_ == Flow::Return) {
      result = std::move(returnValue_);
      returnValue_ = Value();
    } else if (flow_ != Flow::None) {
      flow_ = Flow::None;
      throw NuruError(line, "'vunja' na 'endelea' lazima ziwe ndani ya kitanzi");
    }
    flow_ = Flow::None;
    return result;
  }

  Value Interpreter::evalProperty(const PropertyExpression* e) {
    NURU_EVAL(object, e->object.get())
    return getProperty(object, e->property, e->token.line);
  }

  Value Interpreter::evalAt(const AtExpression* e) {
    if (Value* self = env_->lookup("@")) return *self;
    throw NuruError(e->token.line, "'@' inatumika ndani ya pakeji tu");
  }

  // `tumia name`: a native module, or `name.nr` next to the running script.
}  // namespace mni
