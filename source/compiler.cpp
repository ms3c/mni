#include "compiler.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace mni {

  namespace {
    constexpr const char* kStrayLoopFlow = "'vunja' na 'endelea' lazima ziwe ndani ya kitanzi";
    constexpr const char* kPackageFlow
        = "'rudisha', 'vunja' na 'endelea' haziruhusiwi moja kwa moja ndani ya pakeji";

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
          return TokenType::Modulus;  // ModulusAssign
      }
    }

    // Compiles one proto (a script, a function, a package body or a module).
    class Compiler {
    public:
      explicit Compiler(Proto& proto) : p_(proto) {}

      void compileProgram(const Program& program) {
        for (size_t i = 0; i < program.statements.size(); ++i) {
          const Statement* stmt = program.statements[i].get();
          topLine_ = stmt->token.line;
          // the last statement's value is the value of the whole program
          statement(stmt, i + 1 == program.statements.size());
        }
        if (program.statements.empty()) emit(Op::Null, 1);
        emit(Op::Return, topLine_);
      }

      void compileFunction(const FunctionLiteral& fn) {
        for (const Parameter& param : fn.parameters)
          p_.params.push_back({param.symbol, param.name, param.defaultValue != nullptr});
        prologue(fn);
        block(fn.body.get(), true);  // a function returns the value of its last statement
        emit(Op::Return, fn.body->token.line);
      }

      void compilePackage(const PackageExpression& pkg) {
        packageLine_ = pkg.token.line;
        block(pkg.body.get(), false);
        emit(Op::Null, packageLine_);
        emit(Op::Return, packageLine_);
      }

    private:
      struct Loop {
        int depth;                   // operand stack depth that break and continue return to
        size_t continueTo;           // where continue jumps
        int line;                    // the loop's own line, which its (backward) jumps carry
        std::vector<size_t> breaks;  // Jump instructions to patch to the end of the loop
      };

      // ---- emitting ----

      size_t emit(Op op, int line, int a = 0, int b = 0) {
        p_.code.push_back(Instr{op, a, b});
        p_.lines.push_back(line);
        const StackEffect effect = stackEffect(p_, p_.code.back());
        depth_ -= effect.pops;
        if (depth_ < 0) throw std::logic_error("compiler bug: operand stack underflow");
        depth_ += effect.pushes;
        p_.maxStack = std::max(p_.maxStack, depth_);
        return p_.code.size() - 1;
      }

      // A label: nothing emitted so far may be merged with what comes next.
      size_t here() {
        barrier_ = p_.code.size();
        return barrier_;
      }

      static bool isComparison(int token) {
        switch (static_cast<TokenType>(token)) {
          case TokenType::Lt:
          case TokenType::Lte:
          case TokenType::Gt:
          case TokenType::Gte:
          case TokenType::Eq:
          case TokenType::NotEq:
            return true;
          default:
            return false;
        }
      }

      // A comparison straight before a JumpIfFalse becomes one instruction, unless something
      // jumps to the JumpIfFalse (then the comparison must still leave its result).
      bool fuseBranch() {
        if (p_.code.empty() || barrier_ == p_.code.size()) return false;
        Instr& last = p_.code.back();
        if (last.op == Op::BinOp && isComparison(last.a)) {
          last.op = Op::JumpIfNotCmp;
        } else if (last.op == Op::BinOpK && isComparison(last.a) && last.b < (1 << 22)) {
          last.op = Op::JumpIfNotCmpK;
          last.a |= last.b << 8;
        } else {
          return false;
        }
        last.b = -1;
        --depth_;
        return true;
      }

      size_t emitJump(Op op, int line, int a = 0) {
        if (op == Op::JumpIfFalse && fuseBranch()) return p_.code.size() - 1;
        return emit(op, line, a, -1);
      }

      void patchHere(size_t jump) { p_.code[jump].b = static_cast<int32_t>(here()); }

      int constant(Value value) {
        p_.consts.push_back(std::move(value));
        return static_cast<int>(p_.consts.size() - 1);
      }

      int stringConstant(const std::string& text) { return constant(Value::string(text)); }

      // Code after a Jump/Return/Throw is unreachable, but the surrounding expression is still
      // compiled as if it produced its value, so the depth model stays consistent.
      void unreachable(int depthBefore, bool want) { depth_ = depthBefore + (want ? 1 : 0); }

      void throwError(const std::string& message, int line, bool atCallLine = false) {
        emit(Op::Throw, line, atCallLine ? 1 : 0, stringConstant(message));
      }

      // ---- statements ----

      void block(const BlockStatement* body, bool want) {
        const auto& statements = body->statements;
        for (size_t i = 0; i < statements.size(); ++i)
          statement(statements[i].get(), want && i + 1 == statements.size());
        if (statements.empty() && want) emit(Op::Null, body->token.line);
      }

      void statement(const Statement* stmt, bool want) {
        const int before = depth_;
        const int line = stmt->token.line;
        switch (stmt->kind) {
          case NodeKind::Let: {
            auto* s = static_cast<const LetStatement*>(stmt);
            expression(s->value.get(), true);
            emit(Op::DefineVar, line, 0, static_cast<int>(s->symbol));
            if (want) emit(Op::Null, line);
            break;
          }
          case NodeKind::Return: {
            auto* s = static_cast<const ReturnStatement*>(stmt);
            if (p_.kind == Proto::Kind::Package) {
              throwError(kPackageFlow, packageLine_);
            } else {
              if (s->value) {
                expression(s->value.get(), true);
              } else {
                emit(Op::Null, line);
              }
              emit(Op::Return, line);
            }
            unreachable(before, want);
            break;
          }
          case NodeKind::ExpressionStatement:
            expression(static_cast<const ExpressionStatement*>(stmt)->expression.get(), want);
            break;
          case NodeKind::Block:
            block(static_cast<const BlockStatement*>(stmt), want);
            break;
          case NodeKind::Break:
          case NodeKind::Continue:
            loopFlow(stmt->kind == NodeKind::Break, line);
            unreachable(before, want);
            break;
          default:
            throwError("Kauli isiyojulikana: " + stmt->toString(), line);
            unreachable(before, want);
            break;
        }
        check(before, want);
      }

      // vunja / endelea
      void loopFlow(bool isBreak, int line) {
        if (!loops_.empty()) {
          Loop& loop = loops_.back();
          const int extra = depth_ - loop.depth;
          if (extra > 0) emit(Op::PopN, line, 0, extra);
          if (isBreak) {
            loop.breaks.push_back(emitJump(Op::Jump, line));
          } else {
            emit(Op::Jump, loop.line, 0, static_cast<int>(loop.continueTo));
          }
          return;
        }
        // No loop in this function: the tree-walker reports it where the flow ends up.
        switch (p_.kind) {
          case Proto::Kind::Function:
            throwError(kStrayLoopFlow, line, true);  // at the line of the call
            break;
          case Proto::Kind::Package:
            throwError(kPackageFlow, packageLine_);
            break;
          default:
            throwError(kStrayLoopFlow, topLine_);  // at the top-level statement
            break;
        }
      }

      void check(int before, bool want) const {
        if (depth_ != before + (want ? 1 : 0))
          throw std::logic_error("compiler bug: unbalanced operand stack (" + std::to_string(before)
                                 + " -> " + std::to_string(depth_) + ")");
      }

      // ---- expressions ----

      // Leaves one value on the stack if `want`, otherwise nothing.
      void expression(const Expression* e, bool want) {
        const int before = depth_;
        const int line = e->token.line;
        switch (e->kind) {
          case NodeKind::Identifier:
            emit(Op::GetVar, line, 0, static_cast<int>(static_cast<const Identifier*>(e)->symbol));
            discardIf(!want, line);
            break;
          case NodeKind::IntegerLiteral:
            if (want)
              emit(Op::Const, line, 0,
                   constant(Value::integer(static_cast<const IntegerLiteral*>(e)->value)));
            break;
          case NodeKind::FloatLiteral:
            if (want)
              emit(Op::Const, line, 0,
                   constant(Value::real(static_cast<const FloatLiteral*>(e)->value)));
            break;
          case NodeKind::StringLiteral:
            if (want)
              emit(Op::Const, line, 0,
                   constant(Value::string(static_cast<const StringLiteral*>(e)->shared)));
            break;
          case NodeKind::BooleanLiteral:
            if (want)
              emit(static_cast<const BooleanLiteral*>(e)->value ? Op::True : Op::False, line);
            break;
          case NodeKind::NullLiteral:
            if (want) emit(Op::Null, line);
            break;
          case NodeKind::ArrayLiteral: {
            auto* a = static_cast<const ArrayLiteral*>(e);
            for (const auto& element : a->elements) expression(element.get(), true);
            emit(Op::MakeArray, line, 0, static_cast<int>(a->elements.size()));
            discardIf(!want, line);
            break;
          }
          case NodeKind::DictLiteral:
            dictLiteral(static_cast<const DictLiteral*>(e), want);
            break;
          case NodeKind::Prefix: {
            auto* x = static_cast<const PrefixExpression*>(e);
            expression(x->right.get(), true);
            emit(Op::Prefix, line, static_cast<int>(x->token.type));
            discardIf(!want, line);
            break;
          }
          case NodeKind::Infix:
            infix(static_cast<const InfixExpression*>(e), want);
            break;
          case NodeKind::Postfix:
            postfix(static_cast<const PostfixExpression*>(e), want);
            break;
          case NodeKind::Assign:
            assign(static_cast<const AssignExpression*>(e), want);
            break;
          case NodeKind::If:
            ifExpression(static_cast<const IfExpression*>(e), want);
            break;
          case NodeKind::While:
            whileLoop(static_cast<const WhileExpression*>(e), want);
            break;
          case NodeKind::ForIn:
            forIn(static_cast<const ForInExpression*>(e), want);
            break;
          case NodeKind::Switch:
            switchExpression(static_cast<const SwitchExpression*>(e), want);
            break;
          case NodeKind::Function:
            function(static_cast<const FunctionLiteral*>(e), want);
            break;
          case NodeKind::Call: {
            auto* c = static_cast<const CallExpression*>(e);
            expression(c->callee.get(), true);
            for (const auto& arg : c->arguments) expression(arg.get(), true);
            emit(Op::Call, line, 0, static_cast<int>(c->arguments.size()));
            discardIf(!want, line);
            break;
          }
          case NodeKind::MethodCall:
            methodCall(static_cast<const MethodCallExpression*>(e), want);
            break;
          case NodeKind::Index: {
            auto* x = static_cast<const IndexExpression*>(e);
            expression(x->left.get(), true);
            expression(x->index.get(), true);
            emit(Op::GetIndex, line);
            discardIf(!want, line);
            break;
          }
          case NodeKind::Property: {
            auto* x = static_cast<const PropertyExpression*>(e);
            expression(x->object.get(), true);
            emit(Op::GetProp, line, 0, static_cast<int>(x->symbol));
            discardIf(!want, line);
            break;
          }
          case NodeKind::Import: {
            for (const std::string& name : static_cast<const ImportExpression*>(e)->names)
              emit(Op::Import, line, 0, stringConstant(name));
            if (want) emit(Op::Null, line);
            break;
          }
          case NodeKind::Package: {
            auto* x = static_cast<const PackageExpression*>(e);
            auto body = std::make_unique<Proto>();
            body->kind = Proto::Kind::Package;
            body->name = x->name;
            Compiler(*body).compilePackage(*x);
            p_.children.push_back(std::move(body));
            emit(Op::MakePackage, line, 0, static_cast<int>(p_.children.size() - 1));
            discardIf(!want, line);
            break;
          }
          case NodeKind::At:
            emit(Op::LoadAt, line);
            discardIf(!want, line);
            break;
          default:
            throwError("Bado haijaandikwa: " + e->toString(), line);
            unreachable(before, want);
            break;
        }
        check(before, want);
      }

      void discardIf(bool discard, int line) {
        if (discard) emit(Op::Pop, line);
      }

      void dictLiteral(const DictLiteral* e, bool want) {
        const int line = e->token.line;
        emit(Op::MakeDict, line);
        for (const auto& [key, value] : e->pairs) {
          expression(key.get(), true);
          expression(value.get(), true);
          emit(Op::DictSet, key->token.line);  // a bad key is reported at the key
        }
        discardIf(!want, line);
      }

      void infix(const InfixExpression* e, bool want) {
        const int line = e->token.line;
        const TokenType op = e->token.type;
        if (op == TokenType::And || op == TokenType::Or) {
          // always a boolean, and the right side only runs if it can change the answer
          expression(e->left.get(), true);
          const size_t shortCircuit
              = emitJump(op == TokenType::And ? Op::JumpIfFalse : Op::JumpIfTrue, line);
          const int base = depth_;
          expression(e->right.get(), true);
          emit(Op::Truthy, line);
          const size_t toEnd = emitJump(Op::Jump, line);
          patchHere(shortCircuit);
          depth_ = base;
          emit(op == TokenType::And ? Op::False : Op::True, line);
          patchHere(toEnd);
        } else {
          expression(e->left.get(), true);
          // a constant right operand is part of the instruction
          const size_t before = p_.code.size();
          expression(e->right.get(), true);
          if (p_.code.size() == before + 1 && p_.code.back().op == Op::Const) {
            const int k = p_.code.back().b;
            p_.code.pop_back();
            p_.lines.pop_back();
            --depth_;
            emit(Op::BinOpK, line, static_cast<int>(op), k);
          } else {
            emit(Op::BinOp, line, static_cast<int>(op));
          }
        }
        discardIf(!want, line);
      }

      // `target op= value`, `target = value`. The value stays on the stack.
      void assign(const AssignExpression* e, bool want) {
        const int line = e->token.line;
        const bool compound = e->token.type != TokenType::Assign;
        const int op = static_cast<int>(baseOperator(e->token.type));

        switch (e->target->kind) {
          case NodeKind::Identifier: {
            const int symbol
                = static_cast<int>(static_cast<const Identifier*>(e->target.get())->symbol);
            // a compound assignment reads the old value (and fails if there is none) first
            if (compound) emit(Op::GetVar, line, 0, symbol);
            expression(e->value.get(), true);
            if (compound) emit(Op::BinOp, line, op);
            emit(Op::AssignVar, line, 0, symbol);
            break;
          }
          case NodeKind::Index: {
            auto* target = static_cast<const IndexExpression*>(e->target.get());
            expression(target->left.get(), true);
            expression(target->index.get(), true);
            if (compound) {
              emit(Op::Dup2, line);
              emit(Op::GetIndex, line);
            }
            expression(e->value.get(), true);
            if (compound) emit(Op::BinOp, line, op);
            emit(Op::SetIndex, line);
            break;
          }
          default: {  // Property: the parser accepts only these three targets
            auto* target = static_cast<const PropertyExpression*>(e->target.get());
            const int symbol = static_cast<int>(target->symbol);
            expression(target->object.get(), true);
            if (compound) {
              emit(Op::Dup, line);
              emit(Op::GetProp, line, 0, symbol);
            }
            expression(e->value.get(), true);
            if (compound) emit(Op::BinOp, line, op);
            emit(Op::SetProp, line, 0, symbol);
            break;
          }
        }
        discardIf(!want, line);
      }

      // `x++` / `x--`: the value of the expression is the NEW value.
      void postfix(const PostfixExpression* e, bool want) {
        const int line = e->token.line;
        const Op step = e->token.type == TokenType::PlusPlus ? Op::Incr : Op::Decr;
        switch (e->operand->kind) {
          case NodeKind::Identifier: {
            const int symbol
                = static_cast<int>(static_cast<const Identifier*>(e->operand.get())->symbol);
            emit(Op::GetVar, line, 0, symbol);
            emit(step, line);
            emit(Op::AssignVar, line, 0, symbol);
            break;
          }
          case NodeKind::Index: {
            auto* target = static_cast<const IndexExpression*>(e->operand.get());
            expression(target->left.get(), true);
            expression(target->index.get(), true);
            emit(Op::Dup2, line);
            emit(Op::GetIndex, line);
            emit(step, line);
            emit(Op::SetIndex, line);
            break;
          }
          default: {
            auto* target = static_cast<const PropertyExpression*>(e->operand.get());
            const int symbol = static_cast<int>(target->symbol);
            expression(target->object.get(), true);
            emit(Op::Dup, line);
            emit(Op::GetProp, line, 0, symbol);
            emit(step, line);
            emit(Op::SetProp, line, 0, symbol);
            break;
          }
        }
        discardIf(!want, line);
      }

      void ifExpression(const IfExpression* e, bool want) {
        const int line = e->token.line;
        expression(e->condition.get(), true);
        const size_t toElse = emitJump(Op::JumpIfFalse, line);
        const int base = depth_;
        block(e->consequence.get(), want);
        if (e->alternative || want) {
          const size_t toEnd = emitJump(Op::Jump, line);
          patchHere(toElse);
          depth_ = base;
          if (e->alternative) {
            block(e->alternative.get(), want);
          } else {
            emit(Op::Null, line);  // no else branch: the value is tupu
          }
          patchHere(toEnd);
        } else {
          patchHere(toElse);
        }
      }

      void whileLoop(const WhileExpression* e, bool want) {
        const int line = e->token.line;
        const size_t start = here();
        // The condition belongs to the enclosing loop, not to this one: a vunja inside it is not
        // absorbed here, exactly as in the tree-walker.
        expression(e->condition.get(), true);
        const size_t toEnd = emitJump(Op::JumpIfFalse, line);
        loops_.push_back(Loop{depth_, start, line, {}});
        block(e->body.get(), false);
        emit(Op::Jump, line, 0, static_cast<int>(start));
        patchHere(toEnd);
        for (size_t jump : loops_.back().breaks) patchHere(jump);
        loops_.pop_back();
        if (want) emit(Op::Null, line);
      }

      void forIn(const ForInExpression* e, bool want) {
        const int line = e->token.line;
        expression(e->iterable.get(), true);
        emit(Op::IterInit, line);
        p_.forIns.push_back({e->keySymbol, e->valueSymbol, !e->key.empty()});
        const int info = static_cast<int>(p_.forIns.size() - 1);
        const size_t next = here();
        const size_t iterNext = emit(Op::IterNext, line, info, -1);
        loops_.push_back(
            Loop{depth_, next, line, {}});  // the three hidden slots stay during the body
        block(e->body.get(), false);
        emit(Op::Jump, line, 0, static_cast<int>(next));
        patchHere(iterNext);
        for (size_t jump : loops_.back().breaks) patchHere(jump);
        loops_.pop_back();
        emit(Op::IterPop, line);
        if (want) emit(Op::Null, line);
      }

      void switchExpression(const SwitchExpression* e, bool want) {
        const int line = e->token.line;
        expression(e->value.get(), true);
        const int base = depth_;  // the subject stays on the stack until a body starts

        // Test the cases in source order; `kawaida` runs only if none match.
        std::vector<std::vector<size_t>> matches;
        const SwitchCase* fallback = nullptr;
        for (const SwitchCase& c : e->cases) {
          if (c.isDefault) {
            fallback = &c;
            continue;
          }
          matches.emplace_back();
          for (const auto& value : c.values) {
            expression(value.get(), true);
            matches.back().push_back(emitJump(Op::JumpIfSwitchEq, line));
          }
        }
        const size_t toFallback = emitJump(Op::Jump, line);

        std::vector<size_t> toEnd;
        size_t next = 0;
        for (const SwitchCase& c : e->cases) {
          if (c.isDefault) continue;
          for (size_t jump : matches[next++]) patchHere(jump);
          depth_ = base;
          emit(Op::Pop, line);
          block(c.body.get(), want);
          toEnd.push_back(emitJump(Op::Jump, line));
        }

        patchHere(toFallback);
        depth_ = base;
        emit(Op::Pop, line);
        if (fallback) {
          block(fallback->body.get(), want);
        } else if (want) {
          emit(Op::Null, line);
        }
        for (size_t jump : toEnd) patchHere(jump);
      }

      void function(const FunctionLiteral* e, bool want) {
        const int line = e->token.line;
        auto child = std::make_unique<Proto>();
        child->kind = Proto::Kind::Function;
        child->node = e;
        child->name = e->name;
        Compiler(*child).compileFunction(*e);
        slotify(*child);
        p_.children.push_back(std::move(child));
        emit(Op::MakeFunction, line, 0, static_cast<int>(p_.children.size() - 1));
        // a named function is also defined, after it exists, in the scope that creates it
        if (!e->name.empty()) emit(Op::DefineVarKeep, line, 0, static_cast<int>(e->symbol));
        discardIf(!want, line);
      }

      // Fills in parameters that were not passed, in declaration order, inside the callee's scope.
      void prologue(const FunctionLiteral& fn) {
        if (fn.parameters.empty()) return;
        const int line = fn.token.line;
        const size_t skipAll = emitJump(Op::DefaultsBegin, line);
        for (const Parameter& param : fn.parameters) {
          const size_t given = emitJump(Op::JumpIfOwnDefined, line, static_cast<int>(param.symbol));
          if (param.defaultValue) {
            expression(param.defaultValue.get(), true);
            emit(Op::DefineVar, line, 0, static_cast<int>(param.symbol));
          } else {
            throwError("Kigezo '" + param.name + "' hakijapewa thamani", line, true);
          }
          patchHere(given);
        }
        patchHere(skipAll);
      }

      void methodCall(const MethodCallExpression* e, bool want) {
        const int line = e->token.line;
        expression(e->object.get(), true);
        for (const auto& arg : e->arguments) expression(arg.get(), true);
        CallInfo info{e->methodSymbol, e->method, static_cast<int>(e->arguments.size()), {}};
        for (const NamedArgument& named : e->namedArguments) {
          expression(named.value.get(), true);  // keyword values come after all positional ones
          info.named.push_back(named.name);
        }
        p_.calls.push_back(std::move(info));
        emit(Op::CallMethod, line, 0, static_cast<int>(p_.calls.size() - 1));
        discardIf(!want, line);
      }

      Proto& p_;
      size_t barrier_ = 0;  // the last label's position, see here()
      int depth_ = 0;
      std::vector<Loop> loops_;
      int topLine_ = 1;      // Main/Module: line of the top-level statement being compiled
      int packageLine_ = 1;  // Package: line of the `pakeji` keyword
    };
  }  // namespace

  std::unique_ptr<Proto> compileProgram(const Program& program, Proto::Kind kind) {
    auto proto = std::make_unique<Proto>();
    proto->kind = kind;
    Compiler(*proto).compileProgram(program);
    return proto;
  }

}  // namespace mni
