#include <mni/ast.h>

namespace mni {

  namespace {
    template <class Ptrs> std::string join(const Ptrs& items, const char* sep) {
      std::string out;
      for (size_t i = 0; i < items.size(); ++i) {
        if (i) out += sep;
        out += items[i]->toString();
      }
      return out;
    }

    std::string str(const Node* n) { return n ? n->toString() : "<?>"; }
  }  // namespace

  std::string Program::toString() const { return join(statements, "\n"); }

  std::string BlockStatement::toString() const {
    return statements.empty() ? "{ }" : "{ " + join(statements, " ") + " }";
  }

  std::string LetStatement::toString() const {
    return "fanya " + name + " = " + str(value.get()) + ";";
  }

  std::string ReturnStatement::toString() const {
    return value ? "rudisha " + value->toString() + ";" : "rudisha;";
  }

  std::string ExpressionStatement::toString() const { return str(expression.get()); }

  std::string BreakStatement::toString() const { return "vunja;"; }

  std::string ContinueStatement::toString() const { return "endelea;"; }

  std::string Identifier::toString() const { return name; }

  std::string IntegerLiteral::toString() const { return std::to_string(value); }

  std::string FloatLiteral::toString() const { return token.literal; }

  std::string StringLiteral::toString() const { return "\"" + value + "\""; }

  std::string BooleanLiteral::toString() const { return value ? "kweli" : "sikweli"; }

  std::string NullLiteral::toString() const { return "tupu"; }

  std::string ArrayLiteral::toString() const { return "[" + join(elements, ", ") + "]"; }

  std::string DictLiteral::toString() const {
    std::string out = "{";
    for (size_t i = 0; i < pairs.size(); ++i) {
      if (i) out += ", ";
      out += pairs[i].first->toString() + ": " + pairs[i].second->toString();
    }
    return out + "}";
  }

  std::string PrefixExpression::toString() const { return "(" + op + str(right.get()) + ")"; }

  std::string InfixExpression::toString() const {
    return "(" + str(left.get()) + " " + op + " " + str(right.get()) + ")";
  }

  std::string PostfixExpression::toString() const { return "(" + str(operand.get()) + op + ")"; }

  std::string AssignExpression::toString() const {
    return "(" + str(target.get()) + " " + op + " " + str(value.get()) + ")";
  }

  std::string IfExpression::toString() const {
    std::string out = "kama (" + str(condition.get()) + ") " + str(consequence.get());
    if (alternative) out += " sivyo " + alternative->toString();
    return out;
  }

  std::string WhileExpression::toString() const {
    return "wakati (" + str(condition.get()) + ") " + str(body.get());
  }

  std::string ForInExpression::toString() const {
    return "kwa " + (key.empty() ? "" : key + ", ") + value + " ktk " + str(iterable.get()) + " "
           + str(body.get());
  }

  std::string SwitchExpression::toString() const {
    std::string out = "badili (" + str(value.get()) + ") {";
    for (const auto& c : cases) {
      out += c.isDefault ? " kawaida " : " ikiwa " + join(c.values, ", ") + " ";
      out += str(c.body.get());
    }
    return out + " }";
  }

  std::string FunctionLiteral::toString() const {
    std::string out = "unda " + name + "(";
    for (size_t i = 0; i < parameters.size(); ++i) {
      if (i) out += ", ";
      out += parameters[i].name;
      if (parameters[i].defaultValue) out += " = " + parameters[i].defaultValue->toString();
    }
    return out + ") " + str(body.get());
  }

  std::string CallExpression::toString() const {
    return str(callee.get()) + "(" + join(arguments, ", ") + ")";
  }

  std::string MethodCallExpression::toString() const {
    std::string args = join(arguments, ", ");
    for (const auto& n : namedArguments) {
      if (!args.empty()) args += ", ";
      args += n.name + " = " + n.value->toString();
    }
    return str(object.get()) + "." + method + "(" + args + ")";
  }

  std::string IndexExpression::toString() const {
    return "(" + str(left.get()) + "[" + str(index.get()) + "])";
  }

  std::string PropertyExpression::toString() const {
    return "(" + str(object.get()) + "." + property + ")";
  }

  std::string ImportExpression::toString() const {
    std::string out = "tumia ";
    for (size_t i = 0; i < names.size(); ++i) out += (i ? ", " : "") + names[i];
    return out;
  }

  std::string PackageExpression::toString() const {
    return "pakeji " + name + " " + str(body.get());
  }

  std::string AtExpression::toString() const { return "@"; }

}  // namespace mni
