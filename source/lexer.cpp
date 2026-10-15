#include <mni/lexer.h>

namespace mni {

  namespace {
    bool isLetter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
    bool isDigit(char c) { return c >= '0' && c <= '9'; }
  }  // namespace

  Lexer::Lexer(std::string source) : src_(std::move(source)) {}

  char Lexer::peek(size_t offset) const {
    return pos_ + offset < src_.size() ? src_[pos_ + offset] : '\0';
  }

  char Lexer::advance() {
    char c = peek();
    if (pos_ < src_.size()) ++pos_;
    if (c == '\n') ++line_;
    return c;
  }

  bool Lexer::match(char expected) {
    if (pos_ >= src_.size() || src_[pos_] != expected) return false;
    advance();
    return true;
  }

  Token Lexer::make(TokenType type, std::string literal, int line) const {
    return Token{type, std::move(literal), line};
  }

  void Lexer::skipTrivia() {
    for (;;) {
      char c = peek();
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
        advance();
      } else if (c == '/' && peek(1) == '/') {
        while (peek() != '\n' && pos_ < src_.size()) advance();
      } else if (c == '/' && peek(1) == '*') {
        advance();
        advance();
        while (pos_ < src_.size() && !(peek() == '*' && peek(1) == '/')) advance();
        if (pos_ < src_.size()) {
          advance();
          advance();
        }
      } else if (c == '#' && peek(1) == '!' && pos_ == 0) {
        while (peek() != '\n' && pos_ < src_.size()) advance();
      } else {
        return;
      }
    }
  }

  Token Lexer::readString(char quote) {
    const int line = line_;
    std::string value;
    advance();
    for (;;) {
      if (pos_ >= src_.size()) return make(TokenType::Illegal, std::move(value), line);
      char c = advance();
      if (c == quote) return make(TokenType::String, std::move(value), line);
      if (c != '\\') {
        value += c;
        continue;
      }
      switch (peek()) {
        case 'n':
          value += '\n';
          advance();
          break;
        case 'r':
          value += '\r';
          advance();
          break;
        case 't':
          value += '\t';
          advance();
          break;
        case '"':
          value += '"';
          advance();
          break;
        case '\'':
          value += '\'';
          advance();
          break;
        case '\\':
          value += '\\';
          advance();
          break;
        default:
          value += '\\';
          break;  // unknown escape: keep the backslash
      }
    }
  }

  Token Lexer::readNumber() {
    const int line = line_;
    size_t start = pos_;
    while (isDigit(peek())) advance();
    TokenType type = TokenType::Int;
    if (peek() == '.' && isDigit(peek(1))) {
      advance();
      while (isDigit(peek())) advance();
      type = TokenType::Float;
    }
    return make(type, src_.substr(start, pos_ - start), line);
  }

  Token Lexer::readIdentifier() {
    const int line = line_;
    size_t start = pos_;
    while (isLetter(peek()) || isDigit(peek())) advance();
    std::string word = src_.substr(start, pos_ - start);
    TokenType type = lookupIdent(word);
    return make(type, std::move(word), line);
  }

  Token Lexer::next() {
    skipTrivia();
    const int line = line_;
    if (pos_ >= src_.size()) return make(TokenType::Eof, "", line);

    char c = peek();
    if (isLetter(c)) return readIdentifier();
    if (isDigit(c)) return readNumber();
    if (c == '"' || c == '\'') return readString(c);

    advance();
    auto one = [&](TokenType t) { return make(t, std::string(1, c), line); };
    auto two = [&](TokenType t, char second) { return make(t, std::string{c, second}, line); };

    switch (c) {
      case '=':
        return match('=') ? two(TokenType::Eq, '=') : one(TokenType::Assign);
      case '+':
        if (match('=')) return two(TokenType::PlusAssign, '=');
        if (match('+')) return two(TokenType::PlusPlus, '+');
        return one(TokenType::Plus);
      case '-':
        if (match('=')) return two(TokenType::MinusAssign, '=');
        if (match('-')) return two(TokenType::MinusMinus, '-');
        return one(TokenType::Minus);
      case '*':
        if (match('=')) return two(TokenType::AsteriskAssign, '=');
        if (match('*')) return two(TokenType::Pow, '*');
        return one(TokenType::Asterisk);
      case '/':
        return match('=') ? two(TokenType::SlashAssign, '=') : one(TokenType::Slash);
      case '%':
        return match('=') ? two(TokenType::ModulusAssign, '=') : one(TokenType::Modulus);
      case '!':
        return match('=') ? two(TokenType::NotEq, '=') : one(TokenType::Bang);
      case '<':
        return match('=') ? two(TokenType::Lte, '=') : one(TokenType::Lt);
      case '>':
        return match('=') ? two(TokenType::Gte, '=') : one(TokenType::Gt);
      case '&':
        return match('&') ? two(TokenType::And, '&') : one(TokenType::Illegal);
      case '|':
        return match('|') ? two(TokenType::Or, '|') : one(TokenType::Illegal);
      case ',':
        return one(TokenType::Comma);
      case ';':
        return one(TokenType::Semicolon);
      case '(':
        return one(TokenType::LParen);
      case ')':
        return one(TokenType::RParen);
      case '{':
        return one(TokenType::LBrace);
      case '}':
        return one(TokenType::RBrace);
      case '[':
        return one(TokenType::LBracket);
      case ']':
        return one(TokenType::RBracket);
      case ':':
        return one(TokenType::Colon);
      case '.':
        return one(TokenType::Dot);
      case '@':
        return one(TokenType::At);
      default:
        return one(TokenType::Illegal);
    }
  }

  std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    for (;;) {
      tokens.push_back(next());
      if (tokens.back().type == TokenType::Eof) return tokens;
    }
  }

}  // namespace mni
