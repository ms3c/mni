#include <mni/parser.h>

#include <cctype>
#include <cstdlib>
#include <string>

namespace mni {

  namespace {
    template <class T> std::unique_ptr<T> node(Token token) {
      return std::make_unique<T>(std::move(token));
    }

    bool isAssignable(const Expression* e) {
      return e->kind == NodeKind::Identifier || e->kind == NodeKind::Index
             || e->kind == NodeKind::Property;
    }

    bool isNameToken(const Token& t) {
      return t.type == TokenType::Ident || t.type == TokenType::String;
    }

    // Any word, including keywords, may follow a `.`
    bool isWordToken(const Token& t) {
      return t.type != TokenType::String && !t.literal.empty()
             && (std::isalpha(static_cast<unsigned char>(t.literal[0])) || t.literal[0] == '_');
    }
  }  // namespace

  Parser::Parser(std::string source) : lexer_(std::move(source)) {
    cur_ = lexer_.next();
    peek_ = lexer_.next();
  }

  Parser::Precedence Parser::precedenceOf(TokenType type) {
    switch (type) {
      case TokenType::Assign:
      case TokenType::PlusAssign:
      case TokenType::MinusAssign:
      case TokenType::AsteriskAssign:
      case TokenType::SlashAssign:
      case TokenType::ModulusAssign:
        return Precedence::Assign;
      case TokenType::And:
      case TokenType::Or:
      case TokenType::In:
        return Precedence::Cond;
      case TokenType::Eq:
      case TokenType::NotEq:
        return Precedence::Equals;
      case TokenType::Lt:
      case TokenType::Lte:
      case TokenType::Gt:
      case TokenType::Gte:
        return Precedence::LessGreater;
      case TokenType::Plus:
      case TokenType::Minus:
        return Precedence::Sum;
      case TokenType::Slash:
      case TokenType::Asterisk:
        return Precedence::Product;
      case TokenType::Pow:
        return Precedence::Power;
      case TokenType::Modulus:
        return Precedence::Modulus;
      case TokenType::LParen:
      case TokenType::PlusPlus:
      case TokenType::MinusMinus:
        return Precedence::Call;
      case TokenType::LBracket:
        return Precedence::Index;
      case TokenType::Dot:
        return Precedence::Dot;
      default:
        return Precedence::Lowest;
    }
  }

  void Parser::advance() {
    cur_ = std::move(peek_);
    peek_ = lexer_.next();
  }

  bool Parser::expectPeek(TokenType t) {
    if (peekIs(t)) {
      advance();
      return true;
    }
    errorExpected(t);
    return false;
  }

  void Parser::skipSemicolon() {
    if (peekIs(TokenType::Semicolon)) advance();
  }

  void Parser::error(const std::string& message) {
    errors_.push_back("Mstari " + std::to_string(cur_.line) + ": " + message);
  }

  void Parser::errorExpected(TokenType expected) {
    std::string got(tokenTypeName(peek_.type));
    if (peek_.type == TokenType::Illegal) got += " '" + peek_.literal + "'";
    errors_.push_back("Mstari " + std::to_string(peek_.line) + ": Tulitegemea "
                      + std::string(tokenTypeName(expected)) + ", badala yake tumepata " + got);
  }

  // Skips the rest of the current line (or to a `;`) after a failed statement so one mistake does
  // not produce a cascade of errors. Always makes progress, except at `}` inside a block.
  void Parser::synchronize(bool stopAtBrace) {
    if (curIs(TokenType::Eof)) return;
    if (stopAtBrace && curIs(TokenType::RBrace)) return;
    const int line = cur_.line;
    advance();
    while (!curIs(TokenType::Eof) && cur_.line <= line) {
      if (stopAtBrace && curIs(TokenType::RBrace)) return;
      if (curIs(TokenType::Semicolon)) {
        advance();
        return;
      }
      advance();
    }
  }

  Program Parser::parseProgram() {
    Program program;
    for (;;) {
      while (curIs(TokenType::Semicolon)) advance();
      if (curIs(TokenType::Eof)) break;
      const size_t before = errors_.size();
      StmtPtr stmt = parseStatement();
      if (stmt) {
        program.statements.push_back(std::move(stmt));
        advance();
      } else {
        if (errors_.size() == before) error("Tumeshindwa kuparse");
        synchronize(false);
      }
    }
    return program;
  }

  StmtPtr Parser::parseStatement() {
    switch (cur_.type) {
      case TokenType::Let:
        return parseLet();
      case TokenType::Return:
        return parseReturn();
      case TokenType::Break: {
        auto s = node<BreakStatement>(cur_);
        skipSemicolon();
        return s;
      }
      case TokenType::Continue: {
        auto s = node<ContinueStatement>(cur_);
        skipSemicolon();
        return s;
      }
      default:
        return parseExpressionStatement();
    }
  }

  StmtPtr Parser::parseLet() {
    auto stmt = node<LetStatement>(cur_);
    if (!expectPeek(TokenType::Ident)) return nullptr;
    stmt->name = cur_.literal;
    stmt->symbol = intern(stmt->name);
    if (!expectPeek(TokenType::Assign)) return nullptr;
    advance();
    stmt->value = parseExpression(Precedence::Lowest);
    if (!stmt->value) return nullptr;
    skipSemicolon();
    return stmt;
  }

  StmtPtr Parser::parseReturn() {
    auto stmt = node<ReturnStatement>(cur_);
    if (peekIs(TokenType::Semicolon) || peekIs(TokenType::RBrace) || peekIs(TokenType::Eof)) {
      skipSemicolon();
      return stmt;
    }
    advance();
    stmt->value = parseExpression(Precedence::Lowest);
    if (!stmt->value) return nullptr;
    skipSemicolon();
    return stmt;
  }

  StmtPtr Parser::parseExpressionStatement() {
    auto stmt = node<ExpressionStatement>(cur_);
    stmt->expression = parseExpression(Precedence::Lowest);
    if (!stmt->expression) return nullptr;
    skipSemicolon();
    return stmt;
  }

  std::unique_ptr<BlockStatement> Parser::parseBlock() {
    auto block = node<BlockStatement>(cur_);
    advance();
    for (;;) {
      while (curIs(TokenType::Semicolon)) advance();
      if (curIs(TokenType::RBrace)) return block;
      if (curIs(TokenType::Eof)) {
        error("Hukufunga mabano '}'");
        return nullptr;
      }
      const size_t before = errors_.size();
      StmtPtr stmt = parseStatement();
      if (stmt) {
        block->statements.push_back(std::move(stmt));
        advance();
      } else {
        if (errors_.size() == before) error("Tumeshindwa kuparse");
        synchronize(true);
      }
    }
  }

  ExprPtr Parser::parseExpression(Precedence precedence) {
    constexpr int kMaxDepth = 256;
    if (depth_ >= kMaxDepth) {
      error("Msimbo umejaa viwango vingi mno (zaidi ya " + std::to_string(kMaxDepth) + ")");
      return nullptr;
    }
    struct DepthGuard {
      int& depth;
      explicit DepthGuard(int& d) : depth(d) { ++depth; }
      ~DepthGuard() { --depth; }
    } guard(depth_);

    ExprPtr left = parsePrefix();
    if (!left) return nullptr;
    while (!peekIs(TokenType::Semicolon) && precedence < precedenceOf(peek_.type)) {
      advance();
      left = parseInfix(std::move(left));
      if (!left) return nullptr;
    }
    return left;
  }

  ExprPtr Parser::parsePrefix() {
    switch (cur_.type) {
      case TokenType::Ident: {
        auto id = node<Identifier>(cur_);
        id->name = cur_.literal;
        id->symbol = intern(id->name);
        return id;
      }
      case TokenType::Int:
        return parseInteger();
      case TokenType::Float:
        return parseFloat();
      case TokenType::String: {
        auto s = node<StringLiteral>(cur_);
        s->value = cur_.literal;
        s->shared = std::make_shared<const std::string>(cur_.literal);
        return s;
      }
      case TokenType::True:
      case TokenType::False: {
        auto b = node<BooleanLiteral>(cur_);
        b->value = curIs(TokenType::True);
        return b;
      }
      case TokenType::Null:
        return node<NullLiteral>(cur_);
      case TokenType::At:
        return node<AtExpression>(cur_);
      case TokenType::Bang:
      case TokenType::Minus:
      case TokenType::Plus:
        return parsePrefixOperator();
      case TokenType::LParen:
        return parseGrouped();
      case TokenType::LBracket:
        return parseArray();
      case TokenType::LBrace:
        return parseDict();
      case TokenType::If:
        return parseIf();
      case TokenType::While:
        return parseWhile();
      case TokenType::For:
        return parseForIn();
      case TokenType::Switch:
        return parseSwitch();
      case TokenType::Function:
        return parseFunction();
      case TokenType::Import:
        return parseImport();
      case TokenType::Package:
        return parsePackage();
      default: {
        std::string what(tokenTypeName(cur_.type));
        if (cur_.type == TokenType::Illegal) what += " '" + cur_.literal + "'";
        error("Tumeshindwa kuparse " + what);
        return nullptr;
      }
    }
  }

  ExprPtr Parser::parseInfix(ExprPtr left) {
    switch (cur_.type) {
      case TokenType::Assign:
      case TokenType::PlusAssign:
      case TokenType::MinusAssign:
      case TokenType::AsteriskAssign:
      case TokenType::SlashAssign:
      case TokenType::ModulusAssign:
        return parseAssign(std::move(left));
      case TokenType::LParen:
        return parseCall(std::move(left));
      case TokenType::LBracket:
        return parseIndex(std::move(left));
      case TokenType::Dot:
        return parseDot(std::move(left));
      case TokenType::PlusPlus:
      case TokenType::MinusMinus:
        return parsePostfix(std::move(left));
      default:
        return parseBinary(std::move(left));
    }
  }

  ExprPtr Parser::parseBinary(ExprPtr left) {
    auto expr = node<InfixExpression>(cur_);
    expr->op = cur_.literal;
    expr->left = std::move(left);
    const Precedence precedence = precedenceOf(cur_.type);
    advance();
    expr->right = parseExpression(precedence);
    if (!expr->right) return nullptr;
    return expr;
  }

  ExprPtr Parser::parsePrefixOperator() {
    auto expr = node<PrefixExpression>(cur_);
    expr->op = cur_.literal;
    advance();
    expr->right = parseExpression(Precedence::Prefix);
    if (!expr->right) return nullptr;
    return expr;
  }

  ExprPtr Parser::parseGrouped() {
    advance();
    ExprPtr expr = parseExpression(Precedence::Lowest);
    if (!expr || !expectPeek(TokenType::RParen)) return nullptr;
    return expr;
  }

  ExprPtr Parser::parseInteger() {
    auto lit = node<IntegerLiteral>(cur_);
    try {
      lit->value = std::stoll(cur_.literal);
    } catch (const std::exception&) {
      error("Hatuwezi kuparse '" + cur_.literal + "' kama namba");
      return nullptr;
    }
    return lit;
  }

  ExprPtr Parser::parseFloat() {
    auto lit = node<FloatLiteral>(cur_);
    lit->value = std::strtod(cur_.literal.c_str(), nullptr);
    return lit;
  }

  ExprPtr Parser::parseArray() {
    auto array = node<ArrayLiteral>(cur_);
    if (!parseExpressionList(TokenType::RBracket, array->elements)) return nullptr;
    return array;
  }

  bool Parser::parseExpressionList(TokenType end, std::vector<ExprPtr>& out) {
    if (peekIs(end)) {
      advance();
      return true;
    }
    advance();
    for (;;) {
      ExprPtr expr = parseExpression(Precedence::Lowest);
      if (!expr) return false;
      out.push_back(std::move(expr));
      if (!peekIs(TokenType::Comma)) return expectPeek(end);
      advance();
      if (peekIs(end)) {
        advance();
        return true;
      }
      advance();
    }
  }

  ExprPtr Parser::parseDict() {
    auto dict = node<DictLiteral>(cur_);
    while (!peekIs(TokenType::RBrace)) {
      advance();
      ExprPtr key = parseExpression(Precedence::Lowest);
      if (!key || !expectPeek(TokenType::Colon)) return nullptr;
      advance();
      ExprPtr value = parseExpression(Precedence::Lowest);
      if (!value) return nullptr;
      dict->pairs.emplace_back(std::move(key), std::move(value));
      if (peekIs(TokenType::Comma)) {
        advance();
      } else if (!peekIs(TokenType::RBrace)) {
        errorExpected(TokenType::RBrace);
        return nullptr;
      }
    }
    advance();
    return dict;
  }

  ExprPtr Parser::parseIf() {
    auto expr = node<IfExpression>(cur_);
    if (!expectPeek(TokenType::LParen)) return nullptr;
    advance();
    expr->condition = parseExpression(Precedence::Lowest);
    if (!expr->condition || !expectPeek(TokenType::RParen) || !expectPeek(TokenType::LBrace))
      return nullptr;
    expr->consequence = parseBlock();
    if (!expr->consequence) return nullptr;

    if (peekIs(TokenType::Else)) {
      advance();
      if (peekIs(TokenType::If)) {  // `au kama (...) {...}` chain
        advance();
        auto alternative = node<BlockStatement>(cur_);
        auto stmt = node<ExpressionStatement>(cur_);
        stmt->expression = parseIf();
        if (!stmt->expression) return nullptr;
        alternative->statements.push_back(std::move(stmt));
        expr->alternative = std::move(alternative);
      } else {
        if (!expectPeek(TokenType::LBrace)) return nullptr;
        expr->alternative = parseBlock();
        if (!expr->alternative) return nullptr;
      }
    }
    return expr;
  }

  ExprPtr Parser::parseWhile() {
    auto expr = node<WhileExpression>(cur_);
    if (!expectPeek(TokenType::LParen)) return nullptr;
    advance();
    expr->condition = parseExpression(Precedence::Lowest);
    if (!expr->condition || !expectPeek(TokenType::RParen) || !expectPeek(TokenType::LBrace))
      return nullptr;
    expr->body = parseBlock();
    if (!expr->body) return nullptr;
    return expr;
  }

  ExprPtr Parser::parseForIn() {
    auto expr = node<ForInExpression>(cur_);
    if (!expectPeek(TokenType::Ident)) return nullptr;
    std::string first = cur_.literal;
    if (peekIs(TokenType::Comma)) {
      advance();
      if (!expectPeek(TokenType::Ident)) return nullptr;
      expr->key = std::move(first);
      expr->keySymbol = intern(expr->key);
      expr->value = cur_.literal;
    } else {
      expr->value = std::move(first);
    }
    expr->valueSymbol = intern(expr->value);
    if (!expectPeek(TokenType::In)) return nullptr;
    advance();
    expr->iterable = parseExpression(Precedence::Lowest);
    if (!expr->iterable || !expectPeek(TokenType::LBrace)) return nullptr;
    expr->body = parseBlock();
    if (!expr->body) return nullptr;
    return expr;
  }

  ExprPtr Parser::parseSwitch() {
    auto expr = node<SwitchExpression>(cur_);
    if (!expectPeek(TokenType::LParen)) return nullptr;
    advance();
    expr->value = parseExpression(Precedence::Lowest);
    if (!expr->value || !expectPeek(TokenType::RParen) || !expectPeek(TokenType::LBrace))
      return nullptr;
    advance();

    int defaults = 0;
    while (!curIs(TokenType::RBrace)) {
      if (curIs(TokenType::Eof)) {
        error("Haukufunga 'badili' kwa '}'");
        return nullptr;
      }
      SwitchCase c;
      if (curIs(TokenType::Default)) {
        c.isDefault = true;
      } else if (curIs(TokenType::Case)) {
        if (peekIs(TokenType::Default)) {
          advance();
          c.isDefault = true;
        } else {
          advance();
          for (;;) {
            ExprPtr value = parseExpression(Precedence::Lowest);
            if (!value) return nullptr;
            c.values.push_back(std::move(value));
            if (!peekIs(TokenType::Comma)) break;
            advance();
            advance();
          }
        }
      } else {
        error("Tulitegemea IKIWA au KAWAIDA, badala yake tumepata "
              + std::string(tokenTypeName(cur_.type)));
        return nullptr;
      }
      if (!expectPeek(TokenType::LBrace)) return nullptr;
      c.body = parseBlock();
      if (!c.body) return nullptr;
      advance();
      if (c.isDefault) ++defaults;
      expr->cases.push_back(std::move(c));
    }
    if (defaults > 1) {
      error("'badili' inaruhusiwa kuwa na 'kawaida' moja tu");
      return nullptr;
    }
    return expr;
  }

  ExprPtr Parser::parseFunction() {
    auto fn = node<FunctionLiteral>(cur_);
    if (peekIs(TokenType::Ident)) {
      advance();
      fn->name = cur_.literal;
      fn->symbol = intern(fn->name);
    }
    if (!expectPeek(TokenType::LParen) || !parseParameters(*fn) || !expectPeek(TokenType::LBrace))
      return nullptr;
    fn->body = parseBlock();
    if (!fn->body) return nullptr;
    return fn;
  }

  bool Parser::parseParameters(FunctionLiteral& fn) {
    if (peekIs(TokenType::RParen)) {
      advance();
      return true;
    }
    bool seenDefault = false;
    for (;;) {
      if (!expectPeek(TokenType::Ident)) return false;
      Parameter param;
      param.name = cur_.literal;
      param.symbol = intern(param.name);
      if (peekIs(TokenType::Assign)) {
        advance();
        advance();
        param.defaultValue = parseExpression(Precedence::Lowest);
        if (!param.defaultValue) return false;
        seenDefault = true;
      } else if (seenDefault) {
        error("Kigezo '" + param.name
              + "' lazima kiwe na thamani ya kawaida, kama vilivyotangulia");
        return false;
      }
      fn.parameters.push_back(std::move(param));
      if (!peekIs(TokenType::Comma)) return expectPeek(TokenType::RParen);
      advance();
    }
  }

  ExprPtr Parser::parseImport() {
    auto imp = node<ImportExpression>(cur_);
    for (;;) {
      if (!isNameToken(peek_)) {
        errorExpected(TokenType::Ident);
        return nullptr;
      }
      advance();
      imp->names.push_back(cur_.literal);
      if (peekIs(TokenType::Comma)) {
        advance();
        continue;
      }
      if (isNameToken(peek_) && peek_.line == cur_.line) continue;
      return imp;
    }
  }

  ExprPtr Parser::parsePackage() {
    auto pkg = node<PackageExpression>(cur_);
    if (!expectPeek(TokenType::Ident)) return nullptr;
    pkg->name = cur_.literal;
    if (!expectPeek(TokenType::LBrace)) return nullptr;
    pkg->body = parseBlock();
    if (!pkg->body) return nullptr;
    return pkg;
  }

  ExprPtr Parser::parseCall(ExprPtr callee) {
    auto call = node<CallExpression>(cur_);
    call->callee = std::move(callee);
    if (!parseExpressionList(TokenType::RParen, call->arguments)) return nullptr;
    return call;
  }

  ExprPtr Parser::parseIndex(ExprPtr left) {
    auto index = node<IndexExpression>(cur_);
    index->left = std::move(left);
    advance();
    index->index = parseExpression(Precedence::Lowest);
    if (!index->index || !expectPeek(TokenType::RBracket)) return nullptr;
    return index;
  }

  ExprPtr Parser::parseDot(ExprPtr object) {
    Token dot = cur_;
    if (!isWordToken(peek_)) {
      errorExpected(TokenType::Ident);
      return nullptr;
    }
    advance();
    std::string name = cur_.literal;

    if (!peekIs(TokenType::LParen)) {
      auto prop = node<PropertyExpression>(std::move(dot));
      prop->object = std::move(object);
      prop->property = std::move(name);
      prop->symbol = intern(prop->property);
      return prop;
    }

    auto call = node<MethodCallExpression>(std::move(dot));
    call->object = std::move(object);
    call->method = std::move(name);
    call->methodSymbol = intern(call->method);
    advance();
    if (peekIs(TokenType::RParen)) {
      advance();
      return call;
    }
    advance();
    for (;;) {
      if (curIs(TokenType::Ident) && peekIs(TokenType::Assign)) {
        NamedArgument arg;
        arg.name = cur_.literal;
        advance();
        advance();
        arg.value = parseExpression(Precedence::Lowest);
        if (!arg.value) return nullptr;
        call->namedArguments.push_back(std::move(arg));
      } else {
        ExprPtr arg = parseExpression(Precedence::Lowest);
        if (!arg) return nullptr;
        call->arguments.push_back(std::move(arg));
      }
      if (!peekIs(TokenType::Comma)) {
        if (!expectPeek(TokenType::RParen)) return nullptr;
        return call;
      }
      advance();
      if (peekIs(TokenType::RParen)) {
        advance();
        return call;
      }
      advance();
    }
  }

  ExprPtr Parser::parseAssign(ExprPtr target) {
    if (!isAssignable(target.get())) {
      error("Tulitegemea kitambulishi, array au sifa kabla ya '" + cur_.literal
            + "', badala yake tumepata: " + target->toString());
      return nullptr;
    }
    auto assign = node<AssignExpression>(cur_);
    assign->op = cur_.literal;
    assign->target = std::move(target);
    advance();
    assign->value = parseExpression(Precedence::Lowest);  // right-associative
    if (!assign->value) return nullptr;
    return assign;
  }

  ExprPtr Parser::parsePostfix(ExprPtr operand) {
    if (!isAssignable(operand.get())) {
      error("'" + cur_.literal + "' inaweza kutumika kwa kitambulishi, array au sifa tu");
      return nullptr;
    }
    auto postfix = node<PostfixExpression>(cur_);
    postfix->op = cur_.literal;
    postfix->operand = std::move(operand);
    return postfix;
  }

}  // namespace mni
