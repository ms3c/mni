#pragma once

#include <string>
#include <string_view>

namespace mni {

  /** Token kinds; the comments give the Swahili keyword or name. */
  enum class TokenType {
    Illegal,  // HARAMU
    Eof,      // MWISHO

    // Identifiers + literals
    Ident,   // KITAMBULISHI
    Int,     // NAMBA
    String,  // NENO
    Float,   // DESIMALI

    // Operators
    Assign,
    Plus,
    Minus,
    Bang,
    Asterisk,
    Pow,
    Slash,
    Modulus,
    Lt,
    Lte,
    Gt,
    Gte,
    Eq,
    NotEq,
    And,
    Or,
    PlusAssign,
    PlusPlus,
    MinusAssign,
    MinusMinus,
    AsteriskAssign,
    SlashAssign,
    ModulusAssign,

    // Delimiters
    Comma,
    Semicolon,
    LParen,
    RParen,
    LBrace,
    RBrace,
    LBracket,
    RBracket,
    Colon,
    Dot,
    At,

    // Keywords
    Function,  // unda
    Let,       // fanya
    True,      // kweli
    False,     // sikweli
    If,        // kama
    Else,      // au, sivyo
    Return,    // rudisha
    While,     // wakati
    Null,      // tupu
    Break,     // vunja
    Continue,  // endelea
    In,        // ktk
    For,       // kwa
    Switch,    // badili
    Case,      // ikiwa
    Default,   // kawaida
    Import,    // tumia
    Package,   // pakeji
  };

  struct Token {
    TokenType type = TokenType::Illegal;
    std::string literal;
    int line = 0;
  };

  /** Returns the keyword token type for `ident`, or TokenType::Ident if it is not a keyword. */
  TokenType lookupIdent(std::string_view ident);

  /** Display name of a token type: Nuru's Swahili name for words ("MWISHO"), the symbol for
   * operators. */
  std::string_view tokenTypeName(TokenType type);

}  // namespace mni
