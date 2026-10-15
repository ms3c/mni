#pragma once

#include <mni/symbol.h>
#include <mni/token.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace mni {

  enum class NodeKind {
    // statements
    Let,
    Return,
    ExpressionStatement,
    Block,
    Break,
    Continue,
    // expressions
    Identifier,
    IntegerLiteral,
    FloatLiteral,
    StringLiteral,
    BooleanLiteral,
    NullLiteral,
    ArrayLiteral,
    DictLiteral,
    Prefix,
    Infix,
    Postfix,
    Assign,
    If,
    While,
    ForIn,
    Switch,
    Function,
    Call,
    MethodCall,
    Index,
    Property,
    Import,
    Package,
    At,
  };

  struct Node {
    Node(NodeKind k, Token t) : kind(k), token(std::move(t)) {}
    virtual ~Node() = default;
    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;

    /** Source-like rendering with explicit parentheses, used by tests and `nuru --ast`. */
    virtual std::string toString() const = 0;

    const NodeKind kind;
    const Token token;
  };

  struct Statement : Node {
    using Node::Node;
  };
  struct Expression : Node {
    using Node::Node;
  };

  using StmtPtr = std::unique_ptr<Statement>;
  using ExprPtr = std::unique_ptr<Expression>;

  /** Downcast helper: returns nullptr unless `node` is a T. */
  template <class T, class N> const T* as(const N* node) {
    return node && node->kind == T::Kind ? static_cast<const T*>(node) : nullptr;
  }

#define MNI_NODE(Struct, KindName, Base)                               \
  static constexpr NodeKind Kind = NodeKind::KindName;                 \
  explicit Struct(Token t) : Base(NodeKind::KindName, std::move(t)) {} \
  std::string toString() const override;

  struct BlockStatement : Statement {
    MNI_NODE(BlockStatement, Block, Statement)
    std::vector<StmtPtr> statements;
  };

  struct LetStatement : Statement {
    MNI_NODE(LetStatement, Let, Statement)
    std::string name;
    Symbol symbol = 0;
    ExprPtr value;
  };

  struct ReturnStatement : Statement {
    MNI_NODE(ReturnStatement, Return, Statement)
    ExprPtr value;  // null for a bare `rudisha`
  };

  struct ExpressionStatement : Statement {
    MNI_NODE(ExpressionStatement, ExpressionStatement, Statement)
    ExprPtr expression;
  };

  struct BreakStatement : Statement {
    MNI_NODE(BreakStatement, Break, Statement)
  };

  struct ContinueStatement : Statement {
    MNI_NODE(ContinueStatement, Continue, Statement)
  };

  struct Identifier : Expression {
    MNI_NODE(Identifier, Identifier, Expression)
    std::string name;
    Symbol symbol = 0;
  };

  struct IntegerLiteral : Expression {
    MNI_NODE(IntegerLiteral, IntegerLiteral, Expression)
    int64_t value = 0;
  };

  struct FloatLiteral : Expression {
    MNI_NODE(FloatLiteral, FloatLiteral, Expression)
    double value = 0;
  };

  struct StringLiteral : Expression {
    MNI_NODE(StringLiteral, StringLiteral, Expression)
    std::string value;
    std::shared_ptr<const std::string> shared;  // same text; lets evaluation share, not copy
  };

  struct BooleanLiteral : Expression {
    MNI_NODE(BooleanLiteral, BooleanLiteral, Expression)
    bool value = false;
  };

  struct NullLiteral : Expression {
    MNI_NODE(NullLiteral, NullLiteral, Expression)
  };

  struct ArrayLiteral : Expression {
    MNI_NODE(ArrayLiteral, ArrayLiteral, Expression)
    std::vector<ExprPtr> elements;
  };

  struct DictLiteral : Expression {
    MNI_NODE(DictLiteral, DictLiteral, Expression)
    std::vector<std::pair<ExprPtr, ExprPtr>> pairs;
  };

  struct PrefixExpression : Expression {
    MNI_NODE(PrefixExpression, Prefix, Expression)
    std::string op;
    ExprPtr right;
  };

  struct InfixExpression : Expression {
    MNI_NODE(InfixExpression, Infix, Expression)
    ExprPtr left;
    std::string op;
    ExprPtr right;
  };

  /** `x++` / `x--`; operand is an Identifier, IndexExpression or PropertyExpression. */
  struct PostfixExpression : Expression {
    MNI_NODE(PostfixExpression, Postfix, Expression)
    ExprPtr operand;
    std::string op;
  };

  /** `target op value` where op is one of = += -= *= /= %= and target is assignable. */
  struct AssignExpression : Expression {
    MNI_NODE(AssignExpression, Assign, Expression)
    ExprPtr target;
    std::string op;
    ExprPtr value;
  };

  struct IfExpression : Expression {
    MNI_NODE(IfExpression, If, Expression)
    ExprPtr condition;
    std::unique_ptr<BlockStatement> consequence;
    std::unique_ptr<BlockStatement>
        alternative;  // null, or a block (an `au kama` chain nests an If)
  };

  struct WhileExpression : Expression {
    MNI_NODE(WhileExpression, While, Expression)
    ExprPtr condition;
    std::unique_ptr<BlockStatement> body;
  };

  /** `kwa [key,] value ktk iterable { ... }` */
  struct ForInExpression : Expression {
    MNI_NODE(ForInExpression, ForIn, Expression)
    std::string key;  // empty when there is no key variable
    std::string value;
    Symbol keySymbol = 0;
    Symbol valueSymbol = 0;
    ExprPtr iterable;
    std::unique_ptr<BlockStatement> body;
  };

  struct SwitchCase {
    bool isDefault = false;
    std::vector<ExprPtr> values;
    std::unique_ptr<BlockStatement> body;
  };

  struct SwitchExpression : Expression {
    MNI_NODE(SwitchExpression, Switch, Expression)
    ExprPtr value;
    std::vector<SwitchCase> cases;
  };

  struct Parameter {
    std::string name;
    Symbol symbol = 0;
    ExprPtr defaultValue;  // null when required
  };

  struct FunctionLiteral : Expression {
    MNI_NODE(FunctionLiteral, Function, Expression)
    std::string name;  // empty for anonymous functions
    Symbol symbol = 0;
    std::vector<Parameter> parameters;
    std::unique_ptr<BlockStatement> body;
  };

  struct CallExpression : Expression {
    MNI_NODE(CallExpression, Call, Expression)
    ExprPtr callee;
    std::vector<ExprPtr> arguments;
  };

  struct NamedArgument {
    std::string name;
    ExprPtr value;
  };

  /** `object.method(args, name = value)` */
  struct MethodCallExpression : Expression {
    MNI_NODE(MethodCallExpression, MethodCall, Expression)
    ExprPtr object;
    std::string method;
    Symbol methodSymbol = 0;
    std::vector<ExprPtr> arguments;
    std::vector<NamedArgument> namedArguments;
  };

  struct IndexExpression : Expression {
    MNI_NODE(IndexExpression, Index, Expression)
    ExprPtr left;
    ExprPtr index;
  };

  struct PropertyExpression : Expression {
    MNI_NODE(PropertyExpression, Property, Expression)
    ExprPtr object;
    std::string property;
    Symbol symbol = 0;
  };

  /** `tumia name1, name2` */
  struct ImportExpression : Expression {
    MNI_NODE(ImportExpression, Import, Expression)
    std::vector<std::string> names;
  };

  struct PackageExpression : Expression {
    MNI_NODE(PackageExpression, Package, Expression)
    std::string name;
    std::unique_ptr<BlockStatement> body;
  };

  /** `@`, the current instance inside a package. */
  struct AtExpression : Expression {
    MNI_NODE(AtExpression, At, Expression)
  };

#undef MNI_NODE

  struct Program {
    std::vector<StmtPtr> statements;
    std::string toString() const;
  };

}  // namespace mni
