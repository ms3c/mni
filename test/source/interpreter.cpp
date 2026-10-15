#include <doctest/doctest.h>
#include <mni/interpreter.h>
#include <mni/repl.h>

#include <sstream>
#include <string>

#ifndef _WIN32
#  include <pthread.h>
#endif

using namespace mni;

namespace {
  // Runs `src` and returns everything it printed.
  std::string out(const std::string& src, const std::string& input = "") {
    std::ostringstream os;
    std::istringstream is(input);
    Interpreter interp(os, is);
    interp.run(src);
    return os.str();
  }

  // Runs `src` and returns the inspect() of its last expression value.
  std::string val(const std::string& src) {
    std::ostringstream os;
    std::istringstream is;
    Interpreter interp(os, is);
    return interp.run(src).inspect();
  }

  std::string runtimeError(const std::string& src) {
    std::ostringstream os;
    std::istringstream is;
    Interpreter interp(os, is);
    try {
      interp.run(src);
    } catch (const NuruError& e) {
      return e.what();
    }
    return "<no error>";
  }
}  // namespace

TEST_CASE("arithmetic keeps ints and floats apart") {
  CHECK(val("2 + 3 * 5") == "17");
  CHECK(val("7 - 10") == "-3");
  CHECK(val("6 / 3") == "2");    // exact integer division stays an integer
  CHECK(val("3 / 5") == "0.6");  // otherwise a float
  CHECK(val("2.5 + 3 / 5") == "3.1");
  CHECK(val("10 % 4") == "2");
  CHECK(val("2 ** 10") == "1024");
  CHECK(val("2 ** -1") == "0.5");
  CHECK(val("2 ** 0.5 > 1.41") == "kweli");
  CHECK(val("1 + 1.5") == "2.5");
  CHECK(val("-3 + +2") == "-1");
  CHECK(val("9223372036854775807 + 1") == "9223372036854776000");  // no wraparound: becomes a float
}

TEST_CASE("integers stay exact up to the int64 limit") {
  CHECK(val("9223372036854775807 - 1") == "9223372036854775806");
  CHECK(val("9223372036854775806 + 1") == "9223372036854775807");
  CHECK(val("3037000499 * 3037000499") == "9223372030926249001");  // still fits
  CHECK(val("3037000500 * 3037000500") == "9223372037000250000");  // overflows into a float
  CHECK(val("aina(3037000500 * 3037000500)") == "\"DESIMALI\"");
  CHECK(val("0 - 9223372036854775807 - 2") == "-9223372036854776000");
  CHECK(val("7 % -1") == "0");
  CHECK(val("-7 % 3") == "-1");
}

TEST_CASE("scopes with many variables work in functions and at top level") {
  std::string body, sum;
  for (int i = 0; i < 30; ++i) {
    body += "v" + std::to_string(i) + " = " + std::to_string(i) + "\n";
    sum += (i ? " + v" : "v") + std::to_string(i);
  }
  CHECK(val("f = unda() {\n" + body + "rudisha " + sum + "\n}\nf()") == "435");
  CHECK(val(body + sum) == "435");
}

TEST_CASE("duplicated and recursive calls keep separate argument scopes") {
  CHECK(val("f = unda(a, b = a + 1) { rudisha a * 10 + b } f(1) + f(2)") == "35");
  CHECK(val("g = unda(n) { kama (n == 0) { rudisha 0 } rudisha n + g(n - 1) } g(100)") == "5050");
  // in a plain call, `name = value` is an assignment passed positionally, not a named argument
  CHECK(val("h = unda(x, y) { rudisha x - y } h(y = 1, x = 10)") == "-9");
}

TEST_CASE("floats print in shortest form") {
  CHECK(val("0.1 + 0.2") == "0.30000000000000004");
  CHECK(val("1000.0 * 1000") == "1000000");
  CHECK(val("-2.5") == "-2.5");
  CHECK(val("1.0 / 3") == "0.3333333333333333");
  CHECK(val("10.0 ** 20") == "100000000000000000000");
  CHECK(val("10.0 ** 21") == "1e+21");
  CHECK(val("10.0 ** -7") == "0.0000001");
  CHECK(val("10.0 ** -8") == "1e-8");
  CHECK(val("10.0 ** 25") == "1e+25");
  CHECK(val("0.0") == "0");
}

TEST_CASE("comparison and equality") {
  CHECK(val("1 < 2") == "kweli");
  CHECK(val("2 <= 1") == "sikweli");
  CHECK(val("1 == 1.0") == "kweli");
  CHECK(val("\"a\" == \"a\"") == "kweli");
  CHECK(val("\"a\" < \"b\"") == "kweli");
  CHECK(val("1 == \"1\"") == "sikweli");
  CHECK(val("[1, [2]] == [1, [2]]") == "kweli");
  CHECK(val("{\"a\": 1} == {\"a\": 1}") == "kweli");
  CHECK(val("tupu == tupu") == "kweli");
  CHECK(val("1 != 2") == "kweli");
}

TEST_CASE("truthiness: only tupu and sikweli are false") {
  CHECK(val("!tupu") == "kweli");
  CHECK(val("!sikweli") == "kweli");
  CHECK(val("!0") == "sikweli");
  CHECK(val("!\"\"") == "sikweli");
  CHECK(out("kama (0) { andika(\"ndio\") }") == "ndio\n");
  CHECK(out("kama (tupu) { andika(\"ndio\") } sivyo { andika(\"hapana\") }") == "hapana\n");
}

TEST_CASE("logic operators short-circuit and yield booleans") {
  CHECK(val("kweli && sikweli") == "sikweli");
  CHECK(val("sikweli || kweli") == "kweli");
  CHECK(val("1 && 2") == "kweli");
  CHECK(out("fanya a = 0 sikweli && (a = 1) andika(a)") == "0\n");
  CHECK(out("fanya a = 0 kweli || (a = 1) andika(a)") == "0\n");
}

TEST_CASE("variables: fanya and plain assignment") {
  CHECK(val("fanya x = 5 x") == "5");
  CHECK(val("x = 5 x") == "5");  // no `fanya` needed
  CHECK(val("x = 1 x = x + 1 x") == "2");
  CHECK(val("i = 2 i += 3 i") == "5");
  CHECK(val("i = 6 i /= 2 i") == "3");
  CHECK(val("i = 93 i %= 90 i") == "3");
  CHECK(val("i = 2 i *= 3 i -= 1 i") == "5");
  CHECK(val("a = b = 4 a + b") == "8");
  CHECK(val("i = 2.4 i++ i") == "3.4");
  CHECK(val("i = 0 i++ i++ i--") == "1");
}

TEST_CASE("strings") {
  CHECK(val("\"habari\" + \" \" + \"yako\"") == "\"habari yako\"");
  CHECK(val("\"ab\" * 3") == "\"ababab\"");
  CHECK(val("3 * \"ab\"") == "\"ababab\"");
  CHECK(val("s = \"a\" s += \"b\" s") == "\"ab\"");
  CHECK(val("\"mambo\".idadi()") == "5");
  CHECK(val("\"nuru\".herufikubwa()") == "\"NURU\"");
  CHECK(val("\"NURU\".herufindogo()") == "\"nuru\"");
  CHECK(val("\"nuru mambo  habari\".gawa()") == "[\"nuru\", \"mambo\", \"habari\"]");
  CHECK(val("\"a,b,c\".gawa(\",\")") == "[\"a\", \"b\", \"c\"]");
  CHECK(val("\"abc\".gawa(\"\")") == "[\"a\", \"b\", \"c\"]");
  CHECK(val("\"nuru\"[1]") == "\"u\"");
  CHECK(val("\"nuru\"[-1]") == "\"u\"");
  CHECK(val("\"nuru\"[9]") == "tupu");
  CHECK(val("\"ni\" ktk \"nuru nipo\"") == "kweli");
  CHECK(val("\"\xC3\xA9t\xC3\xA9\".idadi()") == "3");  // UTF-8: counts characters
}

TEST_CASE("arrays") {
  CHECK(val("[1, \"pili\", kweli]") == "[1, \"pili\", kweli]");
  CHECK(val("a = [10, 20, 30] a[1]") == "20");
  CHECK(val("a = [10, 20, 30] a[-1]") == "30");
  CHECK(val("a = [1] a[5]") == "tupu");
  CHECK(val("a = [10, 20, 30] a[1] = 25 a") == "[10, 25, 30]");
  CHECK(val("a = [1, 2] a[0] += 5 a") == "[6, 2]");
  CHECK(val("a = [1, 2] a[1]++ a") == "[1, 3]");
  CHECK(val("[1, 2, 3] + [4, 5, 6]") == "[1, 2, 3, 4, 5, 6]");
  CHECK(val("a = [1] a += [2] a") == "[1, 2]");
  CHECK(val("20 ktk [10, 20, 30]") == "kweli");
  CHECK(val("5 ktk [10, 20, 30]") == "sikweli");
  CHECK(val("[1, 2, 3].idadi()") == "3");
  CHECK(val("a = [1, 2, 3] a.sukuma(\"s\", \"g\") a") == "[1, 2, 3, \"s\", \"g\"]");
  CHECK(val("[1, 2, 3].yamwisho()") == "3");
  CHECK(val("[].yamwisho()") == "tupu");
  CHECK(val("[1, 2, 3].map(unda(x) { rudisha x * 2 })") == "[2, 4, 6]");
  CHECK(val("[1, 2, 3, 4].chuja(unda(x) { rudisha x % 2 == 0 })") == "[2, 4]");
}

TEST_CASE("self-containing values print without looping") {
  CHECK(val("a = [1] a.sukuma(a) a") == "[1, [...]]");
  CHECK(val("d = {\"k\": 1} d[\"self\"] = d d") == "{\"k\": 1, \"self\": {...}}");
  // a value shared twice (not a cycle) is printed in full both times
  CHECK(val("x = [1] y = [x, x] y") == "[[1], [1]]");
  CHECK(val("a = {\"n\": []} b = {\"n\": []} a[\"n\"].sukuma(b) b[\"n\"].sukuma(a) a")
        == "{\"n\": [{\"n\": [{...}]}]}");
}

TEST_CASE("arrays and dicts are shared by reference") {
  CHECK(val("a = [1] b = a b.sukuma(2) a") == "[1, 2]");
  CHECK(val("f = unda(l) { l.sukuma(9) } a = [] f(a) a") == "[9]");
}

TEST_CASE("dictionaries") {
  CHECK(val("{\"jina\": \"Juma\", \"umri\": 25}") == "{\"jina\": \"Juma\", \"umri\": 25}");
  CHECK(val("d = {\"a\": 1} d[\"a\"]") == "1");
  CHECK(val("d = {\"a\": 1} d[\"zz\"]") == "tupu");
  CHECK(val("d = {\"a\": 1} d[\"a\"] = 2 d[\"b\"] = 3 d") == "{\"a\": 2, \"b\": 3}");
  CHECK(val("d = {kweli: \"ndio\", 1: \"moja\", 2.5: \"x\"} d[kweli] + d[1] + d[2.5]")
        == "\"ndiomojax\"");
  CHECK(val("{\"a\": \"apple\"} + {\"b\": \"banana\"}") == "{\"a\": \"apple\", \"b\": \"banana\"}");
  CHECK(val("\"a\" ktk {\"a\": 1}") == "kweli");
  CHECK(val("\"z\" ktk {\"a\": 1}") == "sikweli");
  CHECK(val("d = {\"k\": unda(x) { rudisha x + 1 }} d[\"k\"](1)") == "2");
  CHECK(val("d = {\"n\": 1} d[\"n\"] += 4 d[\"n\"]") == "5");
}

TEST_CASE("functions, defaults, recursion, closures") {
  CHECK(val("jum = unda(x, y) { rudisha x + y } jum(2, 3)") == "5");
  CHECK(val("unda jum(x, y) { rudisha x + y } jum(2, 3)") == "5");
  CHECK(val("f = unda(a, b = 10) { rudisha a + b } f(1)") == "11");
  CHECK(val("f = unda(a, b = 10) { rudisha a + b } f(1, 2)") == "3");
  CHECK(val("f = unda(a, b = a * 2) { rudisha b } f(4)") == "8");
  CHECK(val("f = unda() { 42 } f()") == "42");  // value of the last statement
  CHECK(val("f = unda() { rudisha } f()") == "tupu");
  CHECK(val("f = unda() { rudisha 1 rudisha 2 } f()") == "1");
  CHECK(val("fib = unda(n) { kama (n <= 1) { rudisha n } sivyo { rudisha fib(n-1) + fib(n-2) } } "
            "fib(10)")
        == "55");
  CHECK(val("jum = unda(x) { rudisha unda(y) { rudisha x + y } } jum(5)(3)") == "8");
  CHECK(val("make = unda() { n = 0 rudisha unda() { n++ rudisha n } } c = make() c() c() c()")
        == "3");
}

TEST_CASE("scoping") {
  // assignment updates an existing outer variable...
  CHECK(val("x = 1 f = unda() { x = 2 } f() x") == "2");
  // ...but fanya shadows it
  CHECK(val("x = 1 f = unda() { fanya x = 2 } f() x") == "1");
  // parameters are local
  CHECK(val("x = 1 f = unda(x) { x = 5 } f(9) x") == "1");
  // blocks do not create a scope
  CHECK(val("kama (kweli) { y = 7 } y") == "7");
  // each call has its own locals
  CHECK(val("f = unda() { fanya t = 0 t++ rudisha t } f() f()") == "1");
}

TEST_CASE("if / else chains") {
  const std::string prog
      = "f = unda(n) { kama (n < 0) { rudisha \"hasi\" } au kama (n == 0) { rudisha \"sifuri\" } "
        "sivyo { rudisha \"chanya\" } } ";
  CHECK(val(prog + "f(-1)") == "\"hasi\"");
  CHECK(val(prog + "f(0)") == "\"sifuri\"");
  CHECK(val(prog + "f(5)") == "\"chanya\"");
  CHECK(val("kama (kweli) { 1 }") == "1");
  CHECK(val("kama (sikweli) { 1 }") == "tupu");
}

TEST_CASE("while, break, continue") {
  CHECK(out("i = 1 wakati (i <= 3) { andika(i) i++ }") == "1\n2\n3\n");
  CHECK(out("i = 1 wakati (i < 5) { kama (i == 3) { andika(\"vunja\") vunja } andika(i) i++ }")
        == "1\n2\nvunja\n");
  CHECK(out("i = 0 wakati (i < 5) { i++ kama (i == 3) { andika(\"ruka\") endelea } andika(i) }")
        == "1\n2\nruka\n4\n5\n");
  CHECK(out("i = -2 wakati (i < 2) { andika(i) i++ }") == "-2\n-1\n0\n1\n");
}

TEST_CASE("for-in loops") {
  CHECK(out("kwa v ktk [1, 2, 3] { andika(v) }") == "1\n2\n3\n");
  CHECK(out("kwa i, v ktk [\"juma\", \"asha\"] { andika(i, \"-\", v) }") == "0 - juma\n1 - asha\n");
  CHECK(out("kwa c ktk \"mojo\" { andika(c) }") == "m\no\nj\no\n");
  CHECK(out("kwa i, c ktk \"ab\" { andika(i, \"->\", c) }") == "0 -> a\n1 -> b\n");
  CHECK(out("kwa v ktk {\"a\": \"asili\", \"b\": \"baiskeli\"} { andika(v) }")
        == "asili\nbaiskeli\n");
  CHECK(out("kwa k, v ktk {\"a\": \"asili\", \"b\": \"baiskeli\"} { andika(k, \"=>\", v) }")
        == "a => asili\nb => baiskeli\n");
  CHECK(out("kwa i, c ktk \"mojo\" { kama (i == 2) { andika(\"vunja\") vunja } andika(c) }")
        == "m\no\nvunja\n");
  CHECK(out("kwa i, c ktk \"mojo\" { kama (i == 2) { andika(\"ruka\") endelea } andika(c) }")
        == "m\no\nruka\no\n");
  CHECK(val("f = unda() { kwa v ktk [1, 2, 3] { kama (v == 2) { rudisha v } } } f()") == "2");
  CHECK(out("kwa i ktk mfululizo(3) { andika(i) }") == "0\n1\n2\n");
}

TEST_CASE("for-in iterates a snapshot") {
  CHECK(out("a = [1, 2] kwa v ktk a { a.sukuma(v) } andika(a)") == "[1, 2, 1, 2]\n");
}

TEST_CASE("switch") {
  const std::string prog
      = "f = unda(a) { badili (a) { ikiwa 1, 2 { rudisha \"mdogo\" } ikiwa 3 { rudisha \"tatu\" } "
        "kawaida { rudisha \"nyingine\" } } } ";
  CHECK(val(prog + "f(1)") == "\"mdogo\"");
  CHECK(val(prog + "f(2)") == "\"mdogo\"");
  CHECK(val(prog + "f(3)") == "\"tatu\"");
  CHECK(val(prog + "f(99)") == "\"nyingine\"");
  CHECK(out("badili (2) { ikiwa 3 { andika(\"a\") } }") == "");
  CHECK(out("badili (\"x\") { kawaida { andika(\"d\") } ikiwa \"x\" { andika(\"x\") } }") == "x\n");
}

TEST_CASE("builtins") {
  CHECK(out("andika(1, 2, 3)") == "1 2 3\n");
  CHECK(out("andika()") == "\n");
  CHECK(out("andika(\"a\", [\"b\", 1], {\"k\": \"v\"}, kweli, tupu, 1.5)")
        == "a [\"b\", 1] {\"k\": \"v\"} kweli tupu 1.5\n");
  CHECK(val("aina(2)") == "\"NAMBA\"");
  CHECK(val("aina(2.5)") == "\"DESIMALI\"");
  CHECK(val("aina(\"Nuru\")") == "\"NENO\"");
  CHECK(val("aina([])") == "\"ORODHA\"");
  CHECK(val("aina({})") == "\"KAMUSI\"");
  CHECK(val("aina(kweli)") == "\"BOOLEAN\"");
  CHECK(val("aina(tupu)") == "\"TUPU\"");
  CHECK(val("aina(unda() {})") == "\"KAZI\"");
  CHECK(val("mfululizo(5)") == "[0, 1, 2, 3, 4]");
  CHECK(val("mfululizo(1, 4)") == "[1, 2, 3]");
  CHECK(val("mfululizo(0, 10, 3)") == "[0, 3, 6, 9]");
  CHECK(val("mfululizo(3, 0, -1)") == "[3, 2, 1]");
  CHECK(val("mfululizo(2, 2)") == "[]");
  CHECK(out("n = jaza(\"jina? \") andika(\"hi\", n)", "Juma\nx\n") == "jina? hi Juma\n");
  CHECK(val("jaza()") == "tupu");  // end of input
}

TEST_CASE("toka() asks to exit") {
  std::ostringstream os;
  Interpreter interp(os);
  try {
    interp.run("andika(1) toka(3) andika(2)");
    FAIL("expected ExitRequest");
  } catch (const ExitRequest& e) {
    CHECK(e.code == 3);
  }
  CHECK(os.str() == "1\n");
}

TEST_CASE("runtime errors carry line numbers") {
  CHECK(runtimeError("x") == "Mstari 1: Kitambulishi hakipatikani: x");
  CHECK(runtimeError("1\n\n1 / 0") == "Mstari 3: Huwezi kugawanya kwa sifuri");
  CHECK(runtimeError("1 % 0").find("sifuri") != std::string::npos);
  CHECK(runtimeError("\"a\" + 1") == "Mstari 1: Operesheni haijulikani: NENO + NAMBA");
  CHECK(runtimeError("-\"a\"").find("Operesheni haijulikani") != std::string::npos);
  CHECK(runtimeError("5()") == "Mstari 1: Hii si kazi: NAMBA");
  CHECK(runtimeError("f = unda(a) { } f()").find("hakijapewa") != std::string::npos);
  CHECK(runtimeError("f = unda(a) { } f(1, 2)").find("hoja 1 tu") != std::string::npos);
  CHECK(runtimeError("a = [1] a[\"x\"]").find("lazima iwe NAMBA") != std::string::npos);
  CHECK(runtimeError("a = [1] a[3] = 0").find("nje ya mipaka") != std::string::npos);
  CHECK(runtimeError("5[0]").find("NAMBA") != std::string::npos);
  CHECK(runtimeError("{[1]: 2}").find("hauwezi kuwa ORODHA") != std::string::npos);
  CHECK(runtimeError("s = \"ab\" s[0] = \"x\"").find("NENO") != std::string::npos);
  CHECK(runtimeError("kwa v ktk 5 { }").find("Huwezi kuzunguka") != std::string::npos);
  CHECK(runtimeError("\"a\".haipo()").find("haipo kwa NENO") != std::string::npos);
  CHECK(runtimeError("x = \"s\" x++").find("namba tu") != std::string::npos);
  CHECK(runtimeError("vunja").find("ndani ya kitanzi") != std::string::npos);
  CHECK(runtimeError("f = unda() { vunja } f()").find("ndani ya kitanzi") != std::string::npos);
  CHECK(runtimeError("mfululizo(1, 5, 0)").find("sifuri") != std::string::npos);
  CHECK(runtimeError("[1].map(5)").find("inahitaji kazi") != std::string::npos);
  CHECK(runtimeError("aina()").find("Idadi ya hoja") != std::string::npos);
}

TEST_CASE("deep recursion is an error, not a crash") {
  CHECK(runtimeError("f = unda(n) { rudisha f(n + 1) } f(0)").find("recursion")
        != std::string::npos);
  // the interpreter is still usable afterwards
  std::ostringstream os;
  Interpreter interp(os);
  CHECK_THROWS_AS(interp.run("f = unda() { rudisha f() } f()"), NuruError);
  CHECK(interp.run("1 + 1").inspect() == "2");
}

#ifndef _WIN32
namespace {
  // Windows gives a thread 1 MB of stack; run the interpreter on a thread with the same budget.
  void* runawayRecursion(void* result) {
    std::ostringstream os;
    std::istringstream is;
    Interpreter interp(os, is);
    interp.setMaxCallDepth(50'000);  // more than 1 MB of C++ stack holds, so the tree-walker's
                                     // stack check must stop it
    try {
      interp.run("f = unda(n) { rudisha f(n + 1) } f(0)");
      *static_cast<std::string*>(result) = "<no error>";
    } catch (const NuruError& e) {
      *static_cast<std::string*>(result) = e.what();
    }
    return nullptr;
  }
}  // namespace

TEST_CASE("runaway recursion on a 1 MB stack is an error") {
  std::string result;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr, 1024 * 1024);
  pthread_t thread;
  REQUIRE(pthread_create(&thread, &attr, runawayRecursion, &result) == 0);
  pthread_join(thread, nullptr);
  pthread_attr_destroy(&attr);
  CHECK(result.find("recursion") != std::string::npos);
}
#endif

TEST_CASE("syntax errors are reported as SyntaxError") {
  std::ostringstream os;
  Interpreter interp(os);
  try {
    interp.run("fanya = 1");
    FAIL("expected SyntaxError");
  } catch (const SyntaxError& e) {
    REQUIRE(e.errors.size() == 1);
    CHECK(std::string(e.what()) == e.errors[0]);
  }
}

TEST_CASE("state persists across run() calls and survives errors") {
  std::ostringstream os;
  Interpreter interp(os);
  interp.run("x = 10");
  interp.run("f = unda(n) { rudisha n + x }");
  CHECK_THROWS_AS(interp.run("zzz"), NuruError);
  CHECK_THROWS_AS(interp.run("kwa"), SyntaxError);
  CHECK(interp.run("f(1)").inspect() == "11");
}

TEST_CASE("top-level rudisha stops the script") {
  CHECK(out("andika(1) rudisha 0 andika(2)") == "1\n");
}

TEST_CASE("a realistic program") {
  const std::string src = R"(
    // sum of the even numbers, and word lengths
    unda jumlaWitiri(orodha) {
      s = 0
      kwa n ktk orodha {
        kama (n % 2 != 0) { endelea }
        s += n
      }
      rudisha s
    }
    andika(jumlaWitiri(mfululizo(1, 11)))

    maneno = "habari za asubuhi".gawa()
    urefu = {}
    kwa w ktk maneno { urefu[w] = w.idadi() }
    andika(urefu)

    fanya i = 0
    wakati (kweli) { i++ kama (i >= 3) { vunja } }
    andika(i, aina(i))
  )";
  CHECK(out(src) == "30\n{\"habari\": 6, \"za\": 2, \"asubuhi\": 7}\n3 NAMBA\n");
}

TEST_CASE("repl evaluates lines and prints results") {
  std::istringstream in(
      "1 + 2\nx = 5\nx * 2\nandika(\"hi\")\nunda f(a) {\n  rudisha a + 1\n}\nf(1)\nzzz\nfanya = "
      "3\ntoka(7)\n");
  std::ostringstream os, err;
  Interpreter interp(os, in);
  int code = runRepl(interp, in, os, err);
  CHECK(code == 7);
  CHECK(os.str() == ">>> 3\n>>> 5\n>>> 10\n>>> hi\n>>> ... ... "
                    "unda f(a) {...}\n>>> 2\n>>> >>> >>> ");
  CHECK(err.str().find("Kitambulishi hakipatikani: zzz") != std::string::npos);
  CHECK(err.str().find("Tulitegemea KITAMBULISHI") != std::string::npos);
}
