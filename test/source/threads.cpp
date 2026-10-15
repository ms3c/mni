#include <doctest/doctest.h>
#include <mni/interpreter.h>

#include <sstream>
#include <string>
#include <thread>
#include <vector>

TEST_CASE("independent interpreters can run on separate threads") {
  const std::string program = R"(
tumia hisabati, jsoni
pakeji Counter { n = 0 bump = unda() { @.n++ rudisha @.n } }
fib = unda(n) { kama (n <= 1) { rudisha n } rudisha fib(n - 1) + fib(n - 2) }
t = 0
kwa i ktk mfululizo(200) { t += Counter.bump() + hisabati.floor(hisabati.random() * 0) }
andika(fib(18), t, jsoni.enkodi({"ok": kweli}))
)";

  constexpr int kThreads = 8;
  std::vector<std::string> outputs(kThreads);
  std::vector<std::thread> threads;
  for (int i = 0; i < kThreads; ++i) {
    threads.emplace_back([&, i] {
      std::ostringstream out;
      std::istringstream in;
      mni::Interpreter interp(out, in);
      interp.run(program);
      outputs[static_cast<size_t>(i)] = out.str();
    });
  }
  for (auto& t : threads) t.join();
  for (const auto& output : outputs) CHECK(output == "2584 20100 {\"ok\":true}\n");
}
