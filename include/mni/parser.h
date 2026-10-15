#pragma once

#include <mni/ast.h>
#include <mni/lexer.h>

#include <string>
#include <vector>

namespace mni {

  /**
   * @brief Pratt parser for Nuru
   *
   * Semicolons are optional. Errors are collected, not thrown, as "Mstari N: ..." messages; check
   * errors() after parseProgram(). A statement that fails to parse is dropped from the Program.
   *
   * Precedence, lowest to highest: assignment (right-assoc), `&&` `||` `ktk` (one shared level),
   * `==` `!=`, `<` `<=` `>` `>=`, `+` `-`, `*` `/`, `**`, `%`, prefix `! - +`, call and postfix
   * `++ --`, index, `.`. `%` binding tighter than `**`, and `&&`/`||` sharing a level, match Nuru.
   */
  class Parser {
  public:
    explicit Parser(std::string source);

    Program parseProgram();

    const std::vector<std::string>& errors() const { return errors_; }

  private:
    enum class Precedence {
      Lowest,
      Assign,
      Cond,
      Equals,
      LessGreater,
      Sum,
      Product,
      Power,
      Modulus,
      Prefix,
      Call,
      Index,
      Dot,
    };

    static Precedence precedenceOf(TokenType type);

    void advance();
    bool curIs(TokenType t) const { return cur_.type == t; }
    bool peekIs(TokenType t) const { return peek_.type == t; }
    bool expectPeek(TokenType t);
    void skipSemicolon();
    void error(const std::string& message);
    void errorExpected(TokenType expected);
    void synchronize(bool stopAtBrace);

    StmtPtr parseStatement();
    StmtPtr parseLet();
    StmtPtr parseReturn();
    StmtPtr parseExpressionStatement();
    std::unique_ptr<BlockStatement> parseBlock();  // cur_ is `{`; leaves cur_ on `}`

    ExprPtr parseExpression(Precedence precedence);
    ExprPtr parsePrefix();
    ExprPtr parseInfix(ExprPtr left);
    ExprPtr parseGrouped();
    ExprPtr parseInteger();
    ExprPtr parseFloat();
    ExprPtr parsePrefixOperator();
    ExprPtr parseArray();
    ExprPtr parseDict();
    ExprPtr parseIf();
    ExprPtr parseWhile();
    ExprPtr parseForIn();
    ExprPtr parseSwitch();
    ExprPtr parseFunction();
    ExprPtr parseImport();
    ExprPtr parsePackage();
    ExprPtr parseCall(ExprPtr callee);
    ExprPtr parseIndex(ExprPtr left);
    ExprPtr parseDot(ExprPtr object);
    ExprPtr parseAssign(ExprPtr target);
    ExprPtr parsePostfix(ExprPtr operand);
    ExprPtr parseBinary(ExprPtr left);
    bool parseExpressionList(TokenType end, std::vector<ExprPtr>& out);
    bool parseParameters(FunctionLiteral& fn);

    Lexer lexer_;
    Token cur_;
    Token peek_;
    std::vector<std::string> errors_;
    int depth_ = 0;  // current expression nesting, bounded so deep input cannot overflow the stack
  };

}  // namespace mni
