#include <doctest/doctest.h>
#include <mni/token.h>

using namespace mni;

TEST_CASE("keywords are looked up in Swahili") {
  CHECK(lookupIdent("fanya") == TokenType::Let);
  CHECK(lookupIdent("unda") == TokenType::Function);
  CHECK(lookupIdent("kama") == TokenType::If);
  CHECK(lookupIdent("au") == TokenType::Else);
  CHECK(lookupIdent("sivyo") == TokenType::Else);
  CHECK(lookupIdent("kweli") == TokenType::True);
  CHECK(lookupIdent("tupu") == TokenType::Null);
}

TEST_CASE("non-keywords are identifiers") {
  CHECK(lookupIdent("jina") == TokenType::Ident);
  CHECK(lookupIdent("Fanya") == TokenType::Ident);
}

TEST_CASE("token type names") {
  CHECK(tokenTypeName(TokenType::Eof) == "MWISHO");
  CHECK(tokenTypeName(TokenType::Illegal) == "HARAMU");
}
