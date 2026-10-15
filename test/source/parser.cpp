#include <doctest/doctest.h>
#include <mni/parser.h>

#include <string>
#include <utility>
#include <vector>

using namespace mni;

namespace {
  // Parses `src`, requires no errors, returns Program::toString().
  std::string parse(const std::string& src) {
    Parser parser(src);
    Program program = parser.parseProgram();
    INFO("source: " << src);
    for (const auto& e : parser.errors()) INFO(e);
    REQUIRE(parser.errors().empty());
    return program.toString();
  }

  std::vector<std::string> errorsOf(const std::string& src) {
    Parser parser(src);
    parser.parseProgram();
    return parser.errors();
  }
}  // namespace

TEST_CASE("statements") {
  CHECK(parse("fanya x = 5;") == "fanya x = 5;");
  CHECK(parse("fanya x = 5 fanya y = x") == "fanya x = 5;\nfanya y = x;");
  CHECK(parse("rudisha 1 + 2;") == "rudisha (1 + 2);");
  CHECK(parse("unda f() { rudisha }") == "unda f() { rudisha; }");
  CHECK(parse("vunja endelea") == "vunja;\nendelea;");
  CHECK(parse(";;1;;") == "1");
}

TEST_CASE("literals") {
  CHECK(parse("1") == "1");
  CHECK(parse("2.50") == "2.50");
  CHECK(parse("\"neno\"") == "\"neno\"");
  CHECK(parse("kweli sikweli tupu") == "kweli\nsikweli\ntupu");
  CHECK(parse("[1, 2 + 3, \"x\"]") == "[1, (2 + 3), \"x\"]");
  CHECK(parse("[]") == "[]");
  CHECK(parse("[1,]") == "[1]");
  CHECK(parse("{\"a\": 1, \"b\": [2],}") == "{\"a\": 1, \"b\": [2]}");
  CHECK(parse("fanya d = {}") == "fanya d = {};");
}

TEST_CASE("integer literal values") {
  Parser parser("42");
  Program program = parser.parseProgram();
  auto* stmt = as<ExpressionStatement>(program.statements.at(0).get());
  REQUIRE(stmt);
  auto* lit = as<IntegerLiteral>(stmt->expression.get());
  REQUIRE(lit);
  CHECK(lit->value == 42);
}

TEST_CASE("operator precedence") {
  CHECK(parse("1 + 2 * 3") == "(1 + (2 * 3))");
  CHECK(parse("1 * 2 + 3") == "((1 * 2) + 3)");
  CHECK(parse("1 - 2 - 3") == "((1 - 2) - 3)");
  CHECK(parse("(1 + 2) * 3") == "((1 + 2) * 3)");
  CHECK(parse("-a * b") == "((-a) * b)");
  CHECK(parse("!kweli == sikweli") == "((!kweli) == sikweli)");
  CHECK(parse("a < b == c > d") == "((a < b) == (c > d))");
  CHECK(parse("a + b < c") == "((a + b) < c)");
  CHECK(parse("2 ** 3 ** 2") == "((2 ** 3) ** 2)");  // left-associative, as in Nuru
  CHECK(parse("a ktk b") == "(a ktk b)");
}

TEST_CASE("quirks kept from reference Nuru") {
  CHECK(parse("a % b ** c") == "((a % b) ** c)");    // % binds tighter than **
  CHECK(parse("a || b && c") == "((a || b) && c)");  // && and || share one level
}

TEST_CASE("assignment") {
  CHECK(parse("x = 1") == "(x = 1)");
  CHECK(parse("x = y = 2") == "(x = (y = 2))");
  CHECK(parse("x += 1 + 2") == "(x += (1 + 2))");
  CHECK(parse("x %= 3") == "(x %= 3)");
  CHECK(parse("a[0] = 1") == "((a[0]) = 1)");
  CHECK(parse("a[0] *= 2") == "((a[0]) *= 2)");
  CHECK(parse("o.n = 1") == "((o.n) = 1)");
  CHECK(parse("x = a == b") == "(x = (a == b))");
}

TEST_CASE("postfix") {
  CHECK(parse("i++") == "(i++)");
  CHECK(parse("a[0]--") == "((a[0])--)");
  CHECK(parse("o.n++") == "((o.n)++)");
  CHECK(parse("i++ + 1") == "((i++) + 1)");
}

TEST_CASE("calls, indexing and members") {
  CHECK(parse("f(1, g(2))") == "f(1, g(2))");
  CHECK(parse("f()") == "f()");
  CHECK(parse("a[1][2]") == "((a[1])[2])");
  CHECK(parse("f(x)[0]") == "(f(x)[0])");
  CHECK(parse("o.p") == "(o.p)");
  CHECK(parse("o.p.q") == "((o.p).q)");
  CHECK(parse("o.m(1, 2)") == "o.m(1, 2)");
  CHECK(parse("o.m()") == "o.m()");
  CHECK(parse("o.m(1, k = 2)") == "o.m(1, k = 2)");
  CHECK(parse("a.b(c).d") == "(a.b(c).d)");
  CHECK(parse("-a.b") == "(-(a.b))");
  CHECK(parse("@.jina") == "(@.jina)");
  CHECK(parse("x.kwa") == "(x.kwa)");  // keywords are allowed as member names
}

TEST_CASE("functions") {
  CHECK(parse("unda(x) { x }") == "unda (x) { x }");
  CHECK(parse("unda jumla(a, b) { rudisha a + b }") == "unda jumla(a, b) { rudisha (a + b); }");
  CHECK(parse("unda f(a, b = 2, c = 3) { }") == "unda f(a, b = 2, c = 3) { }");
  CHECK(parse("fanya f = unda() { 1 }; f()") == "fanya f = unda () { 1 };\nf()");
}

TEST_CASE("if / else") {
  CHECK(parse("kama (a) { 1 }") == "kama (a) { 1 }");
  CHECK(parse("kama (a) { 1 } sivyo { 2 }") == "kama (a) { 1 } sivyo { 2 }");
  CHECK(parse("kama (a) { 1 } au { 2 }") == "kama (a) { 1 } sivyo { 2 }");
  CHECK(parse("kama (a) { 1 } au kama (b) { 2 } sivyo { 3 }")
        == "kama (a) { 1 } sivyo { kama (b) { 2 } sivyo { 3 } }");
  CHECK(parse("kama (a) { 1 }\nsivyo { 2 }") == "kama (a) { 1 } sivyo { 2 }");
}

TEST_CASE("loops") {
  CHECK(parse("wakati (i < 3) { i++ }") == "wakati ((i < 3)) { (i++) }");
  CHECK(parse("kwa x ktk xs { x }") == "kwa x ktk xs { x }");
  CHECK(parse("kwa k, v ktk d { v }") == "kwa k, v ktk d { v }");
  CHECK(parse("kwa x ktk f(y) { vunja }") == "kwa x ktk f(y) { vunja; }");
}

TEST_CASE("switch") {
  CHECK(parse("badili (x) { ikiwa 1 { a } ikiwa 2, 3 { b } kawaida { c } }")
        == "badili (x) { ikiwa 1 { a } ikiwa 2, 3 { b } kawaida { c } }");
  CHECK(parse("badili (x) { ikiwa kawaida { c } }") == "badili (x) { kawaida { c } }");
}

TEST_CASE("imports and packages") {
  CHECK(parse("tumia os") == "tumia os");
  CHECK(parse("tumia os, json") == "tumia os, json");
  CHECK(parse("tumia os json") == "tumia os, json");
  CHECK(parse("tumia os\nandika(1)") == "tumia os\nandika(1)");
  CHECK(parse("tumia \"faili.nr\"") == "tumia faili.nr");
  CHECK(parse("pakeji P { fanya x = 1 }") == "pakeji P { fanya x = 1; }");
}

TEST_CASE("a realistic program") {
  const std::string src = R"(
    tumia os
    // sum 1..n
    unda jumla(n) {
      fanya s = 0
      kwa i ktk [1, 2, 3] {
        kama (i % 2 == 0) { endelea }
        s += i
      }
      rudisha s
    }
    fanya orodha = {"a": jumla(3), "b": [kweli, tupu]}
    andika(orodha["a"])
  )";
  Parser parser(src);
  Program program = parser.parseProgram();
  CHECK(parser.errors().empty());
  CHECK(program.statements.size() == 4);
}

TEST_CASE("syntax errors are reported with line numbers") {
  auto e = errorsOf("fanya = 5");
  REQUIRE(e.size() == 1);
  CHECK(e[0] == "Mstari 1: Tulitegemea KITAMBULISHI, badala yake tumepata =");

  e = errorsOf("x = 1\ny = (2\nz = 3");
  REQUIRE_FALSE(e.empty());
  CHECK(e[0].rfind("Mstari 3:", 0) == 0);

  CHECK_FALSE(errorsOf("kama (a) { 1 ").empty());
  CHECK_FALSE(errorsOf("unda f(a = 1, b) { }").empty());
  CHECK_FALSE(errorsOf("1 = 2").empty());
  CHECK_FALSE(errorsOf("f() = 2").empty());
  CHECK_FALSE(errorsOf("1++").empty());
  CHECK_FALSE(errorsOf("kwa i = 0; i < 3; i++ { }").empty());
  CHECK_FALSE(errorsOf("badili (x) { kawaida { } kawaida { } }").empty());
  CHECK_FALSE(errorsOf("badili (x) { 5 }").empty());
  CHECK_FALSE(errorsOf("\"unterminated").empty());
  CHECK_FALSE(errorsOf("a & b").empty());
  CHECK_FALSE(errorsOf(")").empty());
  CHECK_FALSE(errorsOf("}").empty());
}

TEST_CASE("one bad line does not hide later errors or hang") {
  auto e = errorsOf("fanya = 1\nfanya y = 2\nfanya = 3");
  REQUIRE(e.size() == 2);
  CHECK(e[0].rfind("Mstari 1:", 0) == 0);
  CHECK(e[1].rfind("Mstari 3:", 0) == 0);
}

TEST_CASE("absurdly deep nesting is a syntax error, not a crash") {
  CHECK_FALSE(errorsOf(std::string(100000, '(') + "1" + std::string(100000, ')')).empty());
  CHECK_FALSE(errorsOf(std::string(100000, '[')).empty());
  std::string blocks;
  for (int i = 0; i < 5000; ++i) blocks += "kama (kweli) { ";
  CHECK_FALSE(errorsOf(blocks).empty());
  // ordinary nesting is unaffected
  CHECK(errorsOf(std::string(100, '(') + "1" + std::string(100, ')')).empty());
}

TEST_CASE("garbage input terminates") {
  for (const char* src :
       {"{{{{", "((((", "[[[", "kama (", "unda", "tumia", "kwa", "badili", "a.", "}}}", ",,,"}) {
    Parser parser(src);
    parser.parseProgram();
    CHECK_FALSE(parser.errors().empty());
  }
}
