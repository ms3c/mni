#include <doctest/doctest.h>
#include <mni/interpreter.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "generator.h"

// Runs every program in test/corpus and compares what it prints (and how it ends) with the
// frozen `<name>.out` next to it. The frozen files come from the tree-walking evaluator, so they
// are the specification the bytecode VM has to match. Regenerate them after an intended change of
// behavior with MNI_UPDATE_CORPUS=1 (and read the diff).

using namespace mni;
namespace fs = std::filesystem;

namespace {
  std::string environmentVariable(const char* name) {
#ifdef _WIN32
    char* buffer = nullptr;
    size_t size = 0;
    if (_dupenv_s(&buffer, &size, name) != 0 || !buffer) return {};
    std::string value(buffer);
    std::free(buffer);
    return value;
#else
    const char* value = std::getenv(name);
    return value ? value : "";
#endif
  }

  std::string readFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream contents;
    contents << in.rdbuf();
    std::string text = contents.str();
    text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
    return text;
  }

  // Programs named lib_* are modules that other programs import, not programs to run.
  std::vector<fs::path> corpusPrograms() {
    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(MNI_CORPUS_DIR)) {
      const fs::path& path = entry.path();
      if (path.extension() == ".nr" && path.filename().string().rfind("lib_", 0) != 0)
        files.push_back(path);
    }
    std::sort(files.begin(), files.end());
    return files;
  }

  // Everything the program printed, then one line saying how it ended if it did not just finish.
  std::string runProgram(Interpreter::Backend backend, const fs::path& file) {
    std::ostringstream out;
    std::istringstream in;
    Interpreter interp(out, in, Interpreter::Options{backend});
    interp.setScriptPath(file.string());
    try {
      interp.run(readFile(file));
    } catch (const NuruError& e) {
      out << "ERROR: " << e.what() << "\n";
    } catch (const SyntaxError& e) {
      out << "SYNTAX: " << e.what() << "\n";
    } catch (const ExitRequest& e) {
      out << "EXIT: " << e.code << "\n";
    }
    return out.str();
  }

  fs::path expectedFile(const fs::path& program) {
    fs::path expected = program;
    return expected.replace_extension(".out");
  }
}  // namespace

TEST_CASE("the corpus is not empty") { CHECK(corpusPrograms().size() >= 20); }

TEST_CASE("corpus programs print their frozen output on the selected backend") {
  const bool update = !environmentVariable("MNI_UPDATE_CORPUS").empty();
  for (const fs::path& program : corpusPrograms()) {
    INFO("program: " << program.filename().string());
    // only the tree-walker may write the specification
    const std::string actual
        = runProgram(update ? Interpreter::Backend::Tree : Interpreter::Backend::Default, program);
    if (update) {
      std::ofstream(expectedFile(program), std::ios::binary) << actual;
      continue;
    }
    CHECK(actual == readFile(expectedFile(program)));
  }
}

TEST_CASE("both backends agree on every corpus program") {
  if (!Interpreter::vmAvailable()) {
    MESSAGE("the bytecode VM is not built in; nothing to compare");
    return;
  }
  for (const fs::path& program : corpusPrograms()) {
    INFO("program: " << program.filename().string());
    CHECK(runProgram(Interpreter::Backend::Tree, program)
          == runProgram(Interpreter::Backend::Vm, program));
  }
}

TEST_CASE("backend selection") {
  std::ostringstream out;
  std::istringstream in;
  CHECK(Interpreter(out, in, {Interpreter::Backend::Tree}).backend() == Interpreter::Backend::Tree);
  const auto vm = Interpreter(out, in, {Interpreter::Backend::Vm}).backend();
  CHECK(vm == (Interpreter::vmAvailable() ? Interpreter::Backend::Vm : Interpreter::Backend::Tree));
  // the default follows MNI_BACKEND, which the "on vm" registration sets
  const bool wantsVm = environmentVariable("MNI_BACKEND") == "vm" && Interpreter::vmAvailable();
  CHECK(Interpreter(out, in).backend()
        == (wantsVm ? Interpreter::Backend::Vm : Interpreter::Backend::Tree));
}

namespace {
  struct Outcome {
    std::string text;
    bool finished = false;  // run() returned instead of failing
  };

  // Runs source text (not a file) and describes how it ended, like runProgram does for files.
  Outcome runSource(Interpreter::Backend backend, const std::string& source) {
    std::ostringstream out;
    std::istringstream in;
    Interpreter interp(out, in, Interpreter::Options{backend});
    Outcome outcome;
    std::string ending;
    try {
      ending = "result: " + interp.run(source).inspect() + "\n";
      outcome.finished = true;
    } catch (const NuruError& e) {
      ending = std::string("ERROR: ") + e.what() + "\n";
    } catch (const SyntaxError& e) {
      ending = std::string("SYNTAX: ") + e.what() + "\n";
    } catch (const ExitRequest& e) {
      ending = "EXIT: " + std::to_string(e.code) + "\n";
    }
    outcome.text = out.str() + ending;
    return outcome;
  }
}  // namespace

TEST_CASE("random programs behave the same on both backends") {
  if (!Interpreter::vmAvailable()) {
    MESSAGE("the bytecode VM is not built in; nothing to compare");
    return;
  }
  // MNI_FUZZ_COUNT raises the number of programs for a deeper run
  const std::string requested = environmentVariable("MNI_FUZZ_COUNT");
  const uint64_t count = requested.empty() ? 3000 : std::stoull(requested);
  const std::string show = environmentVariable("MNI_FUZZ_SHOW");  // print the first few programs
  const uint64_t shown = show.empty() ? 0 : std::stoull(show);
  uint64_t failed = 0, finished = 0, printed = 0;
  for (uint64_t seed = 1; seed <= count && failed < 5; ++seed) {
    mni::testing::ProgramGenerator generator(seed, true,
                                             seed % 2 == 1);  // odd seeds: mostly numbers
    const std::string source = generator.program(2 + static_cast<int>(seed % 6));
    const Outcome tree = runSource(Interpreter::Backend::Tree, source);
    const Outcome vm = runSource(Interpreter::Backend::Vm, source);
    if (seed <= shown) MESSAGE("seed " << seed << ": " << source << "\n=> " << tree.text);
    if (tree.finished) ++finished;
    if (tree.text.find('\n') != tree.text.size() - 1) ++printed;  // printed before the ending
    if (tree.text != vm.text) {
      ++failed;
      INFO("seed " << seed << "\nprogram: " << source << "\n--- tree:\n"
                   << tree.text << "--- vm:\n"
                   << vm.text);
      CHECK(tree.text == vm.text);
    }
  }
  // the programs must get somewhere, or agreeing proves little
  MESSAGE(finished << " of " << count << " programs ran to the end, " << printed
                   << " printed something");
  CHECK(finished >= count / 10);
}
