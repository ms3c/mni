#pragma once

#include <mni/token.h>

#include <string>
#include <vector>

namespace mni {

  /**
   * @brief Turns Nuru source text into tokens
   *
   * Malformed input (unknown character, lone `&` or `|`, unterminated string) yields an Illegal
   * token instead of throwing. After the end of input every call returns Eof.
   */
  class Lexer {
  public:
    explicit Lexer(std::string source);

    Token next();

    std::vector<Token> tokenize();

  private:
    char peek(size_t offset = 0) const;
    char advance();
    bool match(char expected);
    void skipTrivia();
    Token make(TokenType type, std::string literal, int line) const;
    Token readString(char quote);
    Token readNumber();
    Token readIdentifier();

    std::string src_;
    size_t pos_ = 0;
    int line_ = 1;
  };

}  // namespace mni
