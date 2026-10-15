#include <doctest/doctest.h>
#include <mni/interpreter.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace mni;
namespace fs = std::filesystem;

namespace {
  // A scratch directory removed when the test ends.
  struct TempDir {
    fs::path path;
    TempDir() {
      static int counter = 0;
      path = fs::temp_directory_path()
             / ("mni-test-"
                + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-"
                + std::to_string(counter++));
      fs::create_directories(path);
    }
    ~TempDir() {
      std::error_code ec;
      fs::remove_all(path, ec);
    }
    void write(const std::string& name, const std::string& text) const {
      std::ofstream(path / name, std::ios::binary) << text;
    }
    std::string file(const std::string& name) const { return (path / name).string(); }
  };

  std::string val(const std::string& src) {
    std::ostringstream os;
    std::istringstream is;
    Interpreter interp(os, is);
    return interp.run(src).inspect();
  }

  std::string errorOf(const std::string& src) {
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

  bool contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
  }

  // Runs `src` as if it were the script `dir/main.nr`.
  Value runIn(const TempDir& dir, const std::string& src, std::ostringstream& os) {
    std::istringstream is;
    Interpreter interp(os, is);
    interp.setScriptPath(dir.file("main.nr"));
    return interp.run(src);
  }

  std::string valIn(const TempDir& dir, const std::string& src) {
    std::ostringstream os;
    return runIn(dir, src, os).inspect();
  }

  std::string errorIn(const TempDir& dir, const std::string& src) {
    std::ostringstream os;
    try {
      runIn(dir, src, os);
    } catch (const NuruError& e) {
      return e.what();
    } catch (const SyntaxError& e) {
      return e.what();
    }
    return "<no error>";
  }
}  // namespace

TEST_CASE("pakeji with andaa and @ state") {
  const std::string pkg
      = "pakeji Hesabu {\n"
        "  andaa = unda(n) { @.n = n }\n"
        "  ongeza = unda(x) { rudisha @.n + x }\n"
        "  zidisha = unda() { @.n *= 2 rudisha @.n }\n"
        "}\n";
  CHECK(val(pkg + "Hesabu.andaa(10) Hesabu.ongeza(5)") == "15");
  CHECK(val(pkg + "Hesabu.andaa(10) Hesabu.zidisha() Hesabu.zidisha()") == "40");
  CHECK(val(pkg + "Hesabu.andaa(1) Hesabu.n") == "1");
  CHECK(val(pkg + "Hesabu.n = 3 Hesabu.n += 4 Hesabu.n") == "7");
  CHECK(val(pkg + "Hesabu.n = 1 Hesabu.n++ Hesabu.n") == "2");
  CHECK(val(pkg + "aina(Hesabu)") == "\"PAKEJI\"");
  CHECK(val(pkg + "Hesabu") == "<PAKEJI Hesabu>");
}

TEST_CASE("pakeji members are isolated from the surrounding scope") {
  CHECK(val("x = 1 pakeji P { x = 5 } x") == "1");
  CHECK(val("x = 1 pakeji P { x = 5 } P.x") == "5");
  CHECK(val("pakeji P { f = unda() { y = 9 } } P.f() P.y") == "9");  // y becomes a package variable
  CHECK(contains(errorOf("pakeji P { f = unda() { y = 9 } } P.f() y"),
                 "Kitambulishi hakipatikani: y"));
}

TEST_CASE("two packages keep separate @ state") {
  const std::string src
      = "pakeji A { set = unda(v) { @.v = v } get = unda() { rudisha @.v } }\n"
        "pakeji B { set = unda(v) { @.v = v } get = unda() { rudisha @.v } }\n"
        "A.set(1) B.set(2) ";
  CHECK(val(src + "A.get() * 10 + B.get()") == "12");
}

TEST_CASE("pakeji errors") {
  CHECK(contains(errorOf("@"), "ndani ya pakeji tu"));
  CHECK(contains(errorOf("pakeji P { } P.zzz"), "'zzz' haipo kwenye PAKEJI 'P'"));
  CHECK(contains(errorOf("pakeji P { } P.zzz()"), "'zzz' haipo"));
  CHECK(contains(errorOf("x = 5 x.y"), "Sifa 'y' haipatikani kwenye NAMBA"));
  CHECK(contains(errorOf("x = 5 x.y = 1"), "Huwezi kuweka sifa"));
  CHECK(contains(errorOf("pakeji P { rudisha 1 }"), "haziruhusiwi"));
}

TEST_CASE("named arguments to user functions") {
  const std::string pkg = "pakeji P { f = unda(a, b = 10, c = 100) { rudisha a + b + c } }\n";
  CHECK(val(pkg + "P.f(1)") == "111");
  CHECK(val(pkg + "P.f(1, c = 5)") == "16");
  CHECK(val(pkg + "P.f(c = 1, a = 2)") == "13");
  CHECK(val(pkg + "P.f(1, b = 2, c = 3)") == "6");
  CHECK(contains(errorOf(pkg + "P.f(1, zzz = 2)"), "haina kigezo 'zzz'"));
  CHECK(contains(errorOf(pkg + "P.f(1, a = 2)"), "mara mbili"));
  CHECK(contains(errorOf(pkg + "P.f(b = 2)"), "hakijapewa"));
  CHECK(contains(errorOf("\"a\".idadi(x = 1)"), "hoja zenye majina"));
}

TEST_CASE("tumia loads name.nr next to the script") {
  TempDir dir;
  dir.write("hesabu.nr",
            "pakeji hesabu {\n"
            "  andaa = unda(n) { @.n = n }\n"
            "  jumla = unda(a, b) { rudisha a + b + @.n }\n"
            "}\n");
  CHECK(valIn(dir, "tumia hesabu\nhesabu.andaa(100)\nhesabu.jumla(2, 3)") == "105");
  CHECK(valIn(dir, "tumia hesabu\naina(hesabu)") == "\"PAKEJI\"");
}

TEST_CASE("tumia with a quoted path and several names") {
  TempDir dir;
  dir.write("a.nr", "pakeji a { v = 1 }");
  dir.write("b.nr", "pakeji b { v = 2 }");
  CHECK(valIn(dir, "tumia \"a.nr\"\na.v") == "1");
  CHECK(valIn(dir, "tumia a, b\na.v + b.v") == "3");
  CHECK(valIn(dir, "tumia a b\na.v + b.v") == "3");
}

TEST_CASE("a package file can import others relative to itself") {
  TempDir dir;
  dir.write("util.nr", "pakeji util { mbili = unda(x) { rudisha x * 2 } }");
  dir.write("lib.nr",
            "tumia util\npakeji lib { nne = unda(x) { rudisha util.mbili(util.mbili(x)) } }");
  CHECK(valIn(dir, "tumia lib\nlib.nne(3)") == "12");
}

TEST_CASE("an imported file runs once and is shared") {
  TempDir dir;
  dir.write("counter.nr",
            "andika(\"imepakiwa\")\n"
            "pakeji counter { n = 0 ongeza = unda() { @.n++ rudisha @.n } }\n");
  std::ostringstream os;
  runIn(dir, "tumia counter\ncounter.ongeza()\ntumia counter\ncounter.ongeza()\ncounter.ongeza()",
        os);
  CHECK(os.str() == "imepakiwa\n");
  CHECK(valIn(dir, "tumia counter\ncounter.ongeza() counter.ongeza()") == "2");
}

TEST_CASE("a file's top-level variables do not leak into the importer") {
  TempDir dir;
  dir.write("m.nr", "secret = 42\npakeji m { v = 1 }\n");
  CHECK(contains(errorIn(dir, "tumia m\nsecret"), "Kitambulishi hakipatikani: secret"));
  // and cannot overwrite importer variables
  CHECK(valIn(dir, "secret = 1\ntumia m\nsecret") == "1");
}

TEST_CASE("tumia errors") {
  TempDir dir;
  dir.write("empty.nr", "x = 1\n");
  dir.write("broken.nr", "fanya = \n");
  dir.write("a.nr", "tumia b\npakeji a { }");
  dir.write("b.nr", "tumia a\npakeji b { }");
  CHECK(contains(errorIn(dir, "tumia haipo"), "haipatikani"));
  CHECK(contains(errorIn(dir, "tumia empty"), "halina pakeji 'empty'"));
  CHECK(contains(errorIn(dir, "tumia broken"), "broken.nr"));
  CHECK(contains(errorIn(dir, "tumia a"), "mzunguko"));
}

TEST_CASE("the interpreter recovers after a failed import") {
  TempDir dir;
  dir.write("a.nr", "tumia b\npakeji a { }");
  dir.write("b.nr", "tumia a\npakeji b { }");
  dir.write("ok.nr", "pakeji ok { v = 7 }");
  std::ostringstream os;
  std::istringstream is;
  Interpreter interp(os, is);
  interp.setScriptPath(dir.file("main.nr"));
  CHECK_THROWS_AS(interp.run("tumia a"), NuruError);
  CHECK(interp.run("tumia ok\nok.v").inspect() == "7");
}

TEST_CASE("hisabati constants and functions") {
  CHECK(val("tumia hisabati\nhisabati.PI") == "3.141592653589793");
  CHECK(val("tumia hisabati\nhisabati.e") == "2.718281828459045");
  CHECK(val("tumia hisabati\nhisabati.EPSILON") == "2.220446049250313e-16");
  CHECK(val("tumia hisabati\nhisabati.abs(-42)") == "42");
  CHECK(val("tumia hisabati\nhisabati.abs(-1.5)") == "1.5");
  CHECK(val("tumia hisabati\nhisabati.sqrt(4)") == "2");
  CHECK(val("tumia hisabati\nhisabati.cbrt(8)") == "2");
  CHECK(val("tumia hisabati\nhisabati.root(27, 3)") == "3");
  CHECK(val("tumia hisabati\nhisabati.root(-8, 3)") == "-2");
  CHECK(val("tumia hisabati\nhisabati.ceil(4.3)") == "5");
  CHECK(val("tumia hisabati\nhisabati.floor(4.7)") == "4");
  CHECK(val("tumia hisabati\nhisabati.round(4.6)") == "5");
  CHECK(val("tumia hisabati\nhisabati.round(-2.5)") == "-3");
  CHECK(val("tumia hisabati\nhisabati.sign(-5)") == "-1");
  CHECK(val("tumia hisabati\nhisabati.sign(0)") == "0");
  CHECK(val("tumia hisabati\nhisabati.factorial(5)") == "120");
  CHECK(val("tumia hisabati\nhisabati.factorial(20)") == "2432902008176640000");
  CHECK(val("tumia hisabati\nhisabati.hypot([3, 4])") == "5");
  CHECK(val("tumia hisabati\nhisabati.max([4, 2, 9, 5])") == "9");
  CHECK(val("tumia hisabati\nhisabati.min([4, 2.5, 9])") == "2.5");
  CHECK(val("tumia hisabati\nhisabati.max(1, 7, 3)") == "7");
  CHECK(val("tumia hisabati\nhisabati.cos(0.0)") == "1");
  CHECK(val("tumia hisabati\nhisabati.log2(8)") == "3");
  CHECK(val("tumia hisabati\nhisabati.log10(100.0)") == "2");
  CHECK(val("tumia hisabati\nhisabati.exp(2.0)") == "7.38905609893065");
  // last digits vary between math libraries, so compare with a tolerance
  CHECK(
      val("tumia hisabati\nhisabati.abs(hisabati.acos(0.5) - 1.0471975511965976) < 0.000000000001")
      == "kweli");
  CHECK(val("tumia hisabati\nhisabati.atan2(1.0, 1.0)") == "0.7853981633974483");
  CHECK(val("tumia hisabati\naina(hisabati.sqrt(9))") == "\"DESIMALI\"");
  CHECK(val("tumia hisabati\naina(hisabati.ceil(1.2))") == "\"NAMBA\"");
}

TEST_CASE("hisabati.random gives floats between 0 and 1") {
  CHECK(val("tumia hisabati\n"
            "ok = kweli\n"
            "kwa i ktk mfululizo(200) { r = hisabati.random() kama (r < 0 || r >= 1) { ok = "
            "sikweli } }\n"
            "ok")
        == "kweli");
  CHECK(val("tumia hisabati\nhisabati.random() == hisabati.random()") == "sikweli");
}

TEST_CASE("hisabati errors") {
  CHECK(contains(errorOf("tumia hisabati\nhisabati.sqrt(\"x\")"), "inahitaji namba"));
  CHECK(contains(errorOf("tumia hisabati\nhisabati.sqrt()"), "Idadi ya hoja"));
  CHECK(contains(errorOf("tumia hisabati\nhisabati.max([])"), "angalau namba moja"));
  CHECK(contains(errorOf("tumia hisabati\nhisabati.factorial(-1)"), "isiyo hasi"));
  CHECK(contains(errorOf("tumia hisabati\nhisabati.nope(1)"), "'nope' haipo"));
}

TEST_CASE("jsoni decodes JSON") {
  CHECK(val(R"(tumia jsoni
jsoni.dikodi('{"a": 1, "b": [true, null, 2.5, -3], "c": "x\\ny", "d": {}}'))")
        == R"({"a": 1, "b": [kweli, tupu, 2.5, -3], "c": "x\ny", "d": {}})");
  CHECK(val("tumia jsoni\njsoni.dikodi('[]')") == "[]");
  CHECK(val("tumia jsoni\njsoni.dikodi(' 42 ')") == "42");
  CHECK(val("tumia jsoni\njsoni.dikodi('1e3')") == "1000");
  CHECK(val("tumia jsoni\njsoni.dikodi('\"\\u00e9\\ud83d\\ude00\"')")
        == "\"\xC3\xA9\xF0\x9F\x98\x80\"");
  CHECK(val("tumia jsoni\njsoni.dikodi('12345678901234567890')") == "12345678901234567000");
  CHECK(val("tumia jsoni\nk = jsoni.dikodi('{\"joke\": \"ha\"}')\nk[\"joke\"]") == "\"ha\"");
}

TEST_CASE("jsoni keeps the key order of the document") {
  CHECK(val("tumia jsoni\njsoni.dikodi('{\"z\": 1, \"a\": 2, \"m\": {\"y\": 0, \"b\": 1}}')")
        == "{\"z\": 1, \"a\": 2, \"m\": {\"y\": 0, \"b\": 1}}");
  CHECK(val("tumia jsoni\njsoni.enkodi({\"z\": 1, \"a\": 2})") == "\"{\\\"z\\\":1,\\\"a\\\":2}\"");
}

TEST_CASE("jsoni encodes values") {
  CHECK(val("tumia jsoni\njsoni.enkodi({\"a\": \"apple\", \"b\": [1, kweli, tupu, 2.5]})")
        == R"("{\"a\":\"apple\",\"b\":[1,true,null,2.5]}")");
  CHECK(val("tumia jsoni\njsoni.enkodi(\"a\\\"b\\n\")") == R"("\"a\\\"b\\n\"")");
  CHECK(val("tumia jsoni\njsoni.enkodi({1: 2})") == R"("{\"1\":2}")");
  CHECK(val("tumia jsoni\njsoni.enkodi([])") == "\"[]\"");
  CHECK(val("tumia jsoni\nd = {\"x\": [1, {\"y\": \"z\"}], \"n\": "
            "tupu}\njsoni.dikodi(jsoni.enkodi(d)) == d")
        == "kweli");
}

TEST_CASE("jsoni errors") {
  CHECK(contains(errorOf("tumia jsoni\njsoni.dikodi('{')"), "JSON si sahihi"));
  CHECK(contains(errorOf("tumia jsoni\njsoni.dikodi('[1,]')"), "JSON si sahihi"));
  CHECK(contains(errorOf("tumia jsoni\njsoni.dikodi('{\"a\" 1}')"), "JSON si sahihi"));
  CHECK(contains(errorOf("tumia jsoni\njsoni.dikodi('1 2')"), "JSON si sahihi"));
  CHECK(contains(errorOf("tumia jsoni\njsoni.dikodi('')"), "JSON si sahihi"));
  CHECK(contains(errorOf("tumia jsoni\njsoni.dikodi(5)"), "inahitaji NENO"));
  CHECK(contains(errorOf("tumia jsoni\njsoni.enkodi(unda() {})"), "haiwezi kuwa JSON"));
}

TEST_CASE("muda objects and arithmetic") {
  CHECK(val("tumia muda\naina(muda.hasahivi())") == "\"MUDA\"");
  CHECK(val("tumia muda\nmuda.leo().idadi()") == "10");
  CHECK(val("tumia muda\ns = muda.hasahivi()\nmuda.tofauti(s.ongeza(sekunde = 90), s)") == "90");
  CHECK(val("tumia muda\ns = muda.hasahivi()\nmuda.tofauti(s.ongeza(dakika = 2, sekunde = 5), s)")
        == "125");
  CHECK(val("tumia muda\ns = muda.hasahivi()\nmuda.tofauti(s, s.ongeza(sekunde = 10))") == "-10");
  CHECK(val("tumia muda\nmuda.tofauti(\"10:00:30 05-03-2024\", \"10:00:00 05-03-2024\")") == "30");
  CHECK(val("tumia muda\nmuda.tofauti(muda.baada_ya(100), muda.baada_ya(40)) >= 59") == "kweli");
  CHECK(val("tumia muda\nmuda.tangu(muda.hasahivi()) <= 1") == "kweli");
  CHECK(val("tumia muda\nmuda.tangu(\"00:00:00 01-01-2000\") > 100000000") == "kweli");
  CHECK(val("tumia muda\nmuda.lala(0)") == "tupu");
}

TEST_CASE("muda formats like HH:mm:ss dd-MM-YYYY") {
  std::ostringstream os;
  std::istringstream is;
  Interpreter interp(os, is);
  Value now = interp.run("tumia muda\nmuda.hasahivi()");
  std::string text = now.display();
  REQUIRE(text.size() == 19);
  CHECK(text[2] == ':');
  CHECK(text[5] == ':');
  CHECK(text[8] == ' ');
  CHECK(text[11] == '-');
  CHECK(text[14] == '-');
}

TEST_CASE("muda errors") {
  CHECK(contains(errorOf("tumia muda\nmuda.hasahivi().ongeza()"), "hoja zenye majina"));
  CHECK(contains(errorOf("tumia muda\nmuda.hasahivi().ongeza(foo = 1)"), "haina kigezo 'foo'"));
  CHECK(contains(errorOf("tumia muda\nmuda.hasahivi().ongeza(5)"), "hoja zenye majina"));
  CHECK(contains(errorOf("tumia muda\nmuda.tangu(\"jana\")"), "HH:mm:ss dd-MM-YYYY"));
  CHECK(contains(errorOf("tumia muda\nmuda.tangu(5)"), "inahitaji muda"));
  CHECK(contains(errorOf("tumia muda\nmuda.lala(-1)"), "hasi"));
}

TEST_CASE("fungua and soma read files") {
  TempDir dir;
  dir.write("data.txt", "habari\nyako");
  const std::string path = dir.file("data.txt");
  CHECK(val("f = fungua(\"" + path + "\")\nf.soma()") == "\"habari\\nyako\"");
  CHECK(val("aina(fungua(\"" + path + "\"))") == "\"FAILI\"");
  CHECK(val("fungua(\"" + path + "\").soma().gawa(\"\\n\")") == "[\"habari\", \"yako\"]");
  CHECK(contains(errorOf("fungua(\"/haipo/kabisa.txt\")"), "Imeshindwa kufungua faili"));
  CHECK(contains(errorOf("fungua(5)"), "inahitaji NENO"));
}

TEST_CASE("a realistic package program") {
  TempDir dir;
  dir.write(
      "benki.nr",
      "tumia jsoni\n"
      "pakeji benki {\n"
      "  andaa = unda(jina, salio = 0) { @.jina = jina @.salio = salio }\n"
      "  weka = unda(kiasi) { @.salio += kiasi rudisha @.salio }\n"
      "  toa = unda(kiasi) {\n"
      "    kama (kiasi > @.salio) { rudisha tupu }\n"
      "    @.salio -= kiasi\n"
      "    rudisha @.salio\n"
      "  }\n"
      "  muhtasari = unda() { rudisha jsoni.enkodi({\"jina\": @.jina, \"salio\": @.salio}) }\n"
      "}\n");
  std::ostringstream os;
  runIn(dir,
        "tumia benki, hisabati\n"
        "benki.andaa(\"Asha\", salio = 100)\n"
        "andika(benki.weka(50))\n"
        "andika(benki.toa(500))\n"
        "andika(benki.toa(30))\n"
        "andika(benki.muhtasari())\n"
        "andika(hisabati.max([benki.salio, 10]))\n",
        os);
  CHECK(os.str() == "150\ntupu\n120\n{\"jina\":\"Asha\",\"salio\":120}\n120\n");
}
