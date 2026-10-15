#include <doctest/doctest.h>
#include <mni/interpreter.h>
#include <mni/parser.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "bytecode.h"
#include "compiler.h"
#include "generator.h"

using namespace mni;
namespace fs = std::filesystem;

namespace {
  // The syntax tree has to outlive the proto, which points back into it.
  struct Compiled {
    Program program;
    std::unique_ptr<Proto> proto;
  };

  Compiled compile(const std::string& source, Proto::Kind kind = Proto::Kind::Main) {
    Parser parser(source);
    Compiled result;
    result.program = parser.parseProgram();
    REQUIRE_MESSAGE(parser.errors().empty(), "syntax error in: " << source);
    result.proto = compileProgram(result.program, kind);
    return result;
  }

  // The listing with all runs of spaces collapsed, so the tests do not depend on column widths.
  std::string listing(const std::string& source) {
    Compiled c = compile(source);
    CHECK(verify(*c.proto).empty());
    std::istringstream in(disassemble(*c.proto));
    std::string line, out, word;
    while (std::getline(in, line)) {
      std::istringstream words(line);
      bool first = true;
      while (words >> word) {
        out += (first ? "" : " ") + word;
        first = false;
      }
      out += "\n";
    }
    return out;
  }

  std::string readFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream contents;
    contents << in.rdbuf();
    return contents.str();
  }

  std::vector<fs::path> programsIn(const fs::path& dir) {
    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(dir))
      if (entry.path().extension() == ".nr") files.push_back(entry.path());
    std::sort(files.begin(), files.end());
    return files;
  }
}  // namespace

TEST_CASE("every corpus program and benchmark compiles to well-formed bytecode") {
  std::vector<fs::path> files = programsIn(MNI_CORPUS_DIR);
  for (const fs::path& f : programsIn(MNI_BENCH_DIR)) files.push_back(f);
  REQUIRE(files.size() >= 30);
  for (const fs::path& file : files) {
    INFO("program: " << file.filename().string());
    const bool isModule = file.filename().string().rfind("lib_", 0) == 0;
    Compiled c = compile(readFile(file), isModule ? Proto::Kind::Module : Proto::Kind::Main);
    CHECK(verify(*c.proto) == "");
  }
}

TEST_CASE("bytecode for small program") {
  CHECK(listing("x = 1 + 2 * 3") ==
        "== main (maxStack 3)\n"
        "0 Const #0 1 L1\n"
        "1 Const #1 2 L1\n"
        "2 BinOpK * #2 3 L1\n"
        "3 BinOp + L1\n"
        "4 AssignVar x L1\n"
        "5 Return L1\n");
}

TEST_CASE("bytecode for break inside an operand") {
  CHECK(listing("wakati (a) { y = 1 + kama (b) { vunja } sivyo { 0 } }") ==
        "== main (maxStack 2)\n"
        "0 GetVar a L1\n"
        "1 JumpIfFalse -> 13 L1\n"
        "2 Const #0 1 L1\n"
        "3 GetVar b L1\n"
        "4 JumpIfFalse -> 8 L1\n"
        "5 PopN 1 L1\n"
        "6 Jump -> 13 L1\n"
        "7 Jump -> 9 L1\n"
        "8 Const #1 0 L1\n"
        "9 BinOp + L1\n"
        "10 AssignVar y L1\n"
        "11 Pop L1\n"
        "12 Jump -> 0 L1\n"
        "13 Null L1\n"
        "14 Return L1\n");
}

TEST_CASE("bytecode for and or") {
  CHECK(listing("r = a && b || c") ==
        "== main (maxStack 1)\n"
        "0 GetVar a L1\n"
        "1 JumpIfFalse -> 5 L1\n"
        "2 GetVar b L1\n"
        "3 Truthy L1\n"
        "4 Jump -> 6 L1\n"
        "5 False L1\n"
        "6 JumpIfTrue -> 10 L1\n"
        "7 GetVar c L1\n"
        "8 Truthy L1\n"
        "9 Jump -> 11 L1\n"
        "10 True L1\n"
        "11 AssignVar r L1\n"
        "12 Return L1\n");
}

TEST_CASE("bytecode for switch default first") {
  CHECK(listing("badili (x) { kawaida { 0 } ikiwa 1, 2 { 10 } ikiwa 3 { 30 } }") ==
        "== main (maxStack 2)\n"
        "0 GetVar x L1\n"
        "1 Const #0 1 L1\n"
        "2 JumpIfSwitchEq -> 8 L1\n"
        "3 Const #1 2 L1\n"
        "4 JumpIfSwitchEq -> 8 L1\n"
        "5 Const #2 3 L1\n"
        "6 JumpIfSwitchEq -> 11 L1\n"
        "7 Jump -> 14 L1\n"
        "8 Pop L1\n"
        "9 Const #3 10 L1\n"
        "10 Jump -> 16 L1\n"
        "11 Pop L1\n"
        "12 Const #4 30 L1\n"
        "13 Jump -> 16 L1\n"
        "14 Pop L1\n"
        "15 Const #5 0 L1\n"
        "16 Return L1\n");
}

TEST_CASE("bytecode for for in") {
  CHECK(listing("kwa i, v ktk xs { kama (v) { endelea } wakati (kama (i) { vunja } sivyo { kweli }) { } vunja }") ==
        "== main (maxStack 4)\n"
        "0 GetVar xs L1\n"
        "1 IterInit L1\n"
        "2 IterNext -> 15 L1\n"
        "3 GetVar v L1\n"
        "4 JumpIfFalse -> 6 L1\n"
        "5 Jump -> 2 L1\n"
        "6 GetVar i L1\n"
        "7 JumpIfFalse -> 10 L1\n"
        "8 Jump -> 15 L1\n"
        "9 Jump -> 11 L1\n"
        "10 True L1\n"
        "11 JumpIfFalse -> 13 L1\n"
        "12 Jump -> 6 L1\n"
        "13 Jump -> 15 L1\n"
        "14 Jump -> 2 L1\n"
        "15 IterPop L1\n"
        "16 Null L1\n"
        "17 Return L1\n");
}

TEST_CASE("bytecode for defaults") {
  CHECK(listing("f = unda(a, b = a * 2) { rudisha a + b }") ==
        "== main (maxStack 1)\n"
        "0 MakeFunction <anonymous> L1\n"
        "1 AssignVar f L1\n"
        "2 Return L1\n"
        "== function (maxStack 2)\n"
        "0 DefaultsBegin -> 7 L1\n"
        "1 JumpIfSlotSet $0 a -> 3 L1\n"
        "2 Throw (call line) \"Kigezo 'a' hakijapewa thamani\" L1\n"
        "3 JumpIfSlotSet $1 b -> 7 L1\n"
        "4 GetSlot $0 a L1\n"
        "5 BinOpK * #1 2 L1\n"
        "6 DefineSlot $1 b L1\n"
        "7 GetSlot $0 a L1\n"
        "8 GetSlot $1 b L1\n"
        "9 BinOp + L1\n"
        "10 Return L1\n"
        "11 Return L1\n");
}

TEST_CASE("bytecode for compound") {
  CHECK(listing("a[i()] += 1\no.p *= 2\nx++\na[0]--") ==
        "== main (maxStack 4)\n"
        "0 GetVar a L1\n"
        "1 GetVar i L1\n"
        "2 Call 0 L1\n"
        "3 Dup2 L1\n"
        "4 GetIndex L1\n"
        "5 Const #0 1 L1\n"
        "6 BinOp + L1\n"
        "7 SetIndex L1\n"
        "8 Pop L1\n"
        "9 GetVar o L2\n"
        "10 Dup L2\n"
        "11 GetProp p L2\n"
        "12 Const #1 2 L2\n"
        "13 BinOp * L2\n"
        "14 SetProp p L2\n"
        "15 Pop L2\n"
        "16 GetVar x L3\n"
        "17 Incr L3\n"
        "18 AssignVar x L3\n"
        "19 Pop L3\n"
        "20 GetVar a L4\n"
        "21 Const #2 0 L4\n"
        "22 Dup2 L4\n"
        "23 GetIndex L4\n"
        "24 Decr L4\n"
        "25 SetIndex L4\n"
        "26 Return L4\n");
}

TEST_CASE("bytecode for package and calls") {
  CHECK(listing("tumia a, b\npakeji P { n = 1 }\nd = {\"k\": 1}\nP.f(1, z = 2)\nxs.map(g)") ==
        "== main (maxStack 3)\n"
        "0 Import a L1\n"
        "1 Import b L1\n"
        "2 MakePackage P L2\n"
        "3 Pop L2\n"
        "4 MakeDict L3\n"
        "5 Const #2 \"k\" L3\n"
        "6 Const #3 1 L3\n"
        "7 DictSet L3\n"
        "8 AssignVar d L3\n"
        "9 Pop L3\n"
        "10 GetVar P L4\n"
        "11 Const #4 1 L4\n"
        "12 Const #5 2 L4\n"
        "13 CallMethod f 1 z= L4\n"
        "14 Pop L4\n"
        "15 GetVar xs L5\n"
        "16 GetVar g L5\n"
        "17 CallMethod map 1 L5\n"
        "18 Return L5\n"
        "== package P (maxStack 1)\n"
        "0 Const #0 1 L2\n"
        "1 AssignVar n L2\n"
        "2 Pop L2\n"
        "3 Null L2\n"
        "4 Return L2\n");
}

TEST_CASE("random programs compile to well-formed bytecode") {
  // break, continue and return end up inside expression operands, defaults and switch cases
  // here; the verifier checks that the compiler's idea of the operand stack always matches.
  int compiled = 0;
  for (uint64_t seed = 1; seed <= 4000; ++seed) {
    mni::testing::ProgramGenerator generator(seed);
    const std::string source = generator.program(1 + static_cast<int>(seed % 6));
    Parser parser(source);
    Program program = parser.parseProgram();
    if (!parser.errors().empty()) continue;  // the generator may produce an occasional bad shape
    ++compiled;
    INFO("seed " << seed << ": " << source);
    std::unique_ptr<Proto> proto;
    REQUIRE_NOTHROW(proto = compileProgram(program, Proto::Kind::Main));
    CHECK(verify(*proto) == "");
  }
  CHECK(compiled >= 3000);  // most programs must parse, or the test proves little
}
