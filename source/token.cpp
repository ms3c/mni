#include <mni/token.h>

#include <unordered_map>

namespace mni {

  TokenType lookupIdent(std::string_view ident) {
    static const std::unordered_map<std::string_view, TokenType> keywords{
        {"unda", TokenType::Function},    {"fanya", TokenType::Let},
        {"kweli", TokenType::True},       {"sikweli", TokenType::False},
        {"kama", TokenType::If},          {"au", TokenType::Else},
        {"sivyo", TokenType::Else},       {"wakati", TokenType::While},
        {"rudisha", TokenType::Return},   {"vunja", TokenType::Break},
        {"endelea", TokenType::Continue}, {"tupu", TokenType::Null},
        {"ktk", TokenType::In},           {"kwa", TokenType::For},
        {"badili", TokenType::Switch},    {"ikiwa", TokenType::Case},
        {"kawaida", TokenType::Default},  {"tumia", TokenType::Import},
        {"pakeji", TokenType::Package},
    };
    auto it = keywords.find(ident);
    return it == keywords.end() ? TokenType::Ident : it->second;
  }

  std::string_view tokenTypeName(TokenType type) {
    switch (type) {
      case TokenType::Illegal:
        return "HARAMU";
      case TokenType::Eof:
        return "MWISHO";
      case TokenType::Ident:
        return "KITAMBULISHI";
      case TokenType::Int:
        return "NAMBA";
      case TokenType::String:
        return "NENO";
      case TokenType::Float:
        return "DESIMALI";
      case TokenType::Assign:
        return "=";
      case TokenType::Plus:
        return "+";
      case TokenType::Minus:
        return "-";
      case TokenType::Bang:
        return "!";
      case TokenType::Asterisk:
        return "*";
      case TokenType::Pow:
        return "**";
      case TokenType::Slash:
        return "/";
      case TokenType::Modulus:
        return "%";
      case TokenType::Lt:
        return "<";
      case TokenType::Lte:
        return "<=";
      case TokenType::Gt:
        return ">";
      case TokenType::Gte:
        return ">=";
      case TokenType::Eq:
        return "==";
      case TokenType::NotEq:
        return "!=";
      case TokenType::And:
        return "&&";
      case TokenType::Or:
        return "||";
      case TokenType::PlusAssign:
        return "+=";
      case TokenType::PlusPlus:
        return "++";
      case TokenType::MinusAssign:
        return "-=";
      case TokenType::MinusMinus:
        return "--";
      case TokenType::AsteriskAssign:
        return "*=";
      case TokenType::SlashAssign:
        return "/=";
      case TokenType::ModulusAssign:
        return "%=";
      case TokenType::Comma:
        return ",";
      case TokenType::Semicolon:
        return ";";
      case TokenType::LParen:
        return "(";
      case TokenType::RParen:
        return ")";
      case TokenType::LBrace:
        return "{";
      case TokenType::RBrace:
        return "}";
      case TokenType::LBracket:
        return "[";
      case TokenType::RBracket:
        return "]";
      case TokenType::Colon:
        return ":";
      case TokenType::Dot:
        return ".";
      case TokenType::At:
        return "@";
      case TokenType::Function:
        return "FUNCTION";
      case TokenType::Let:
        return "FANYA";
      case TokenType::True:
        return "KWELI";
      case TokenType::False:
        return "SIKWELI";
      case TokenType::If:
        return "KAMA";
      case TokenType::Else:
        return "SIVYO";
      case TokenType::Return:
        return "RUDISHA";
      case TokenType::While:
        return "WAKATI";
      case TokenType::Null:
        return "TUPU";
      case TokenType::Break:
        return "VUNJA";
      case TokenType::Continue:
        return "ENDELEA";
      case TokenType::In:
        return "KTK";
      case TokenType::For:
        return "KWA";
      case TokenType::Switch:
        return "BADILI";
      case TokenType::Case:
        return "IKIWA";
      case TokenType::Default:
        return "KAWAIDA";
      case TokenType::Import:
        return "TUMIA";
      case TokenType::Package:
        return "PAKEJI";
    }
    return "?";
  }

}  // namespace mni
