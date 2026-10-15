#include <doctest/doctest.h>
#include <mni/lexer.h>

#include <utility>
#include <vector>

using namespace mni;

namespace {
  using Expected = std::pair<TokenType, std::string>;

  std::vector<Expected> lex(const std::string& src) {
    std::vector<Expected> out;
    for (const Token& t : Lexer(src).tokenize()) out.emplace_back(t.type, t.literal);
    return out;
  }
}  // namespace

TEST_CASE("lexes a variable declaration") {
  CHECK(lex("fanya x = 5;")
        == std::vector<Expected>{{TokenType::Let, "fanya"},
                                 {TokenType::Ident, "x"},
                                 {TokenType::Assign, "="},
                                 {TokenType::Int, "5"},
                                 {TokenType::Semicolon, ";"},
                                 {TokenType::Eof, ""}});
}

TEST_CASE("lexes multi-character operators") {
  CHECK(lex("== != <= >= && || ** += -= *= /= %= ++ --")
        == std::vector<Expected>{{TokenType::Eq, "=="},
                                 {TokenType::NotEq, "!="},
                                 {TokenType::Lte, "<="},
                                 {TokenType::Gte, ">="},
                                 {TokenType::And, "&&"},
                                 {TokenType::Or, "||"},
                                 {TokenType::Pow, "**"},
                                 {TokenType::PlusAssign, "+="},
                                 {TokenType::MinusAssign, "-="},
                                 {TokenType::AsteriskAssign, "*="},
                                 {TokenType::SlashAssign, "/="},
                                 {TokenType::ModulusAssign, "%="},
                                 {TokenType::PlusPlus, "++"},
                                 {TokenType::MinusMinus, "--"},
                                 {TokenType::Eof, ""}});
}

TEST_CASE("lexes delimiters") {
  CHECK(lex("( ) { } [ ] , ; : . @")
        == std::vector<Expected>{{TokenType::LParen, "("},
                                 {TokenType::RParen, ")"},
                                 {TokenType::LBrace, "{"},
                                 {TokenType::RBrace, "}"},
                                 {TokenType::LBracket, "["},
                                 {TokenType::RBracket, "]"},
                                 {TokenType::Comma, ","},
                                 {TokenType::Semicolon, ";"},
                                 {TokenType::Colon, ":"},
                                 {TokenType::Dot, "."},
                                 {TokenType::At, "@"},
                                 {TokenType::Eof, ""}});
}

TEST_CASE("every keyword") {
  CHECK(lex("unda fanya kweli sikweli kama au sivyo wakati rudisha vunja endelea tupu ktk kwa "
            "badili ikiwa kawaida tumia pakeji")
        == std::vector<Expected>{{TokenType::Function, "unda"},    {TokenType::Let, "fanya"},
                                 {TokenType::True, "kweli"},       {TokenType::False, "sikweli"},
                                 {TokenType::If, "kama"},          {TokenType::Else, "au"},
                                 {TokenType::Else, "sivyo"},       {TokenType::While, "wakati"},
                                 {TokenType::Return, "rudisha"},   {TokenType::Break, "vunja"},
                                 {TokenType::Continue, "endelea"}, {TokenType::Null, "tupu"},
                                 {TokenType::In, "ktk"},           {TokenType::For, "kwa"},
                                 {TokenType::Switch, "badili"},    {TokenType::Case, "ikiwa"},
                                 {TokenType::Default, "kawaida"},  {TokenType::Import, "tumia"},
                                 {TokenType::Package, "pakeji"},   {TokenType::Eof, ""}});
}

TEST_CASE("numbers") {
  CHECK(lex("7 3.14 5.")
        == std::vector<Expected>{{TokenType::Int, "7"},
                                 {TokenType::Float, "3.14"},
                                 {TokenType::Int, "5"},
                                 {TokenType::Dot, "."},
                                 {TokenType::Eof, ""}});
}

TEST_CASE("strings") {
  auto t = lex(R"("a b" 'c' "x\ny" "q\"q" 'it\'s' "back\\slash")");
  CHECK(t[0] == Expected{TokenType::String, "a b"});
  CHECK(t[1] == Expected{TokenType::String, "c"});
  CHECK(t[2] == Expected{TokenType::String, "x\ny"});
  CHECK(t[3] == Expected{TokenType::String, "q\"q"});
  CHECK(t[4] == Expected{TokenType::String, "it's"});
  CHECK(t[5] == Expected{TokenType::String, "back\\slash"});
  CHECK(t[6].first == TokenType::Eof);
}

TEST_CASE("UTF-8 passes through strings") {
  CHECK(lex("\"habari za asubuhi \xE2\x9C\x93\"")[0]
        == Expected{TokenType::String, "habari za asubuhi \xE2\x9C\x93"});
}

TEST_CASE("comments and shebang are skipped") {
  CHECK(lex("#!/usr/bin/env nuru\n// line\nx /* block\nspanning */ y")
        == std::vector<Expected>{
            {TokenType::Ident, "x"}, {TokenType::Ident, "y"}, {TokenType::Eof, ""}});
  // `#!` is only a shebang at the very start
  CHECK(lex("x\n#!")[1].first == TokenType::Illegal);
}

TEST_CASE("line numbers") {
  auto tokens = Lexer("a\n\n b /*\n*/ c \"s\ns\" d").tokenize();
  REQUIRE(tokens.size() == 6);
  CHECK(tokens[0].line == 1);
  CHECK(tokens[1].line == 3);
  CHECK(tokens[2].line == 4);
  CHECK(tokens[3].line == 4);  // string starts on line 4
  CHECK(tokens[4].line == 5);  // after the newline inside the string
}

TEST_CASE("bad input becomes Illegal tokens") {
  CHECK(lex("&")[0].first == TokenType::Illegal);
  CHECK(lex("|")[0].first == TokenType::Illegal);
  CHECK(lex("$")[0].first == TokenType::Illegal);
  CHECK(lex("\"never closed")[0].first == TokenType::Illegal);
}

TEST_CASE("Eof is sticky") {
  Lexer l("");
  CHECK(l.next().type == TokenType::Eof);
  CHECK(l.next().type == TokenType::Eof);
}
