#include <doctest/doctest.h>
#include <mni/interpreter.h>

#include <atomic>
#include <chrono>
#include <sstream>
#include <thread>

using namespace mni;

namespace {
  struct Rig {
    std::ostringstream out;
    std::istringstream in;
    Interpreter interp{out, in};
  };
}  // namespace

TEST_CASE("a tick limit stops an endless loop") {
  Rig r;
  r.interp.setLimits({.maxTicks = 5000});
  try {
    r.interp.run("wakati (kweli) { }");
    FAIL("expected a LimitError");
  } catch (const LimitError& e) {
    CHECK(e.kind() == LimitError::Kind::Ticks);
  }
}

TEST_CASE("a program under the tick limit runs, and the limit applies per run") {
  Rig r;
  r.interp.setLimits({.maxTicks = 3000});
  for (int i = 0; i < 3; ++i) {
    CHECK_NOTHROW(r.interp.run("i = 0; wakati (i < 2000) { i++ }"));
  }
  CHECK_THROWS_AS(r.interp.run("i = 0; wakati (i < 4000) { i++ }"), LimitError);
  // the interpreter is usable afterwards
  r.interp.setLimits({});
  CHECK(r.interp.run("1 + 1").inspect() == "2");
}

TEST_CASE("endless recursion under a tick limit") {
  Rig r;
  r.interp.setMaxCallDepth(1'000'000);
  r.interp.setLimits(
      {.maxTicks
       = 200});  // few enough that the tree-walker is not stopped by its stack check first
  CHECK_THROWS_AS(r.interp.run("f = unda(n) { rudisha f(n + 1) } f(0)"), LimitError);
}

TEST_CASE("a timeout stops an endless loop") {
  Rig r;
  r.interp.setLimits({.timeout = std::chrono::milliseconds(50)});
  try {
    r.interp.run("wakati (kweli) { }");
    FAIL("expected a LimitError");
  } catch (const LimitError& e) {
    CHECK(e.kind() == LimitError::Kind::Timeout);
  }
}

TEST_CASE("a cancel flag set from another thread stops the program") {
  Rig r;
  std::atomic<bool> cancel{false};
  r.interp.setLimits({.cancel = &cancel});
  std::thread t([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    cancel = true;
  });
  try {
    r.interp.run("wakati (kweli) { }");
    t.join();
    FAIL("expected a LimitError");
  } catch (const LimitError& e) {
    t.join();
    CHECK(e.kind() == LimitError::Kind::Cancelled);
  }
}

TEST_CASE("for-in loops count ticks too") {
  Rig r;
  r.interp.run("xs = []; i = 0; wakati (i < 500) { xs.sukuma(i); i++ }");
  r.interp.setLimits({.maxTicks = 100});
  CHECK_THROWS_AS(r.interp.run("kwa v ktk xs { }"), LimitError);
}
