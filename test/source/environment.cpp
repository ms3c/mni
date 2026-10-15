#include <doctest/doctest.h>
#include <mni/environment.h>
#include <mni/symbol.h>

#include <string>

using namespace mni;

TEST_CASE("symbols are interned") {
  const Symbol a = intern("symbol_test_alpha");
  CHECK(intern("symbol_test_alpha") == a);
  CHECK(intern("symbol_test_beta") != a);
  CHECK(symbolName(a) == "symbol_test_alpha");
  CHECK(symbolCount() > a);
}

TEST_CASE("a scope sees its own and outer variables, and can shadow") {
  auto outer = std::make_shared<Environment>();
  outer->define("x", Value::integer(1));
  Environment inner(outer);
  REQUIRE(inner.lookup("x"));
  CHECK(inner.lookup("x")->asInt() == 1);
  CHECK(inner.lookupOwn("x") == nullptr);
  inner.define("x", Value::integer(2));
  CHECK(inner.lookup("x")->asInt() == 2);
  CHECK(outer->lookup("x")->asInt() == 1);
  CHECK(inner.lookup("missing") == nullptr);
}

TEST_CASE("assign updates the nearest binding or creates one here") {
  auto outer = std::make_shared<Environment>();
  outer->define("x", Value::integer(1));
  Environment inner(outer);
  inner.assign("x", Value::integer(5));
  CHECK(outer->lookup("x")->asInt() == 5);
  CHECK(inner.lookupOwn("x") == nullptr);
  inner.assign("fresh", Value::integer(7));
  CHECK(inner.lookupOwn("fresh")->asInt() == 7);
  CHECK(outer->lookup("fresh") == nullptr);
}

TEST_CASE("assign never crosses a boundary scope") {
  auto globals = std::make_shared<Environment>();
  globals->define("g", Value::integer(1));
  Environment pkg(globals);
  pkg.setBoundary();
  pkg.assign("g", Value::integer(2));
  CHECK(globals->lookup("g")->asInt() == 1);
  CHECK(pkg.lookupOwn("g")->asInt() == 2);
  pkg.assign("fresh", Value::integer(3));
  CHECK(globals->lookup("fresh") == nullptr);
}

TEST_CASE("a scope with many variables keeps every one reachable") {
  Environment env;
  for (int i = 0; i < 200; ++i) env.define("many_" + std::to_string(i), Value::integer(i));
  for (int i = 0; i < 200; ++i) {
    Value* v = env.lookupOwn("many_" + std::to_string(i));
    REQUIRE(v);
    CHECK(v->asInt() == i);
  }
  env.define("many_5", Value::integer(-5));
  env.assign("many_150", Value::integer(-150));
  CHECK(env.lookupOwn("many_5")->asInt() == -5);
  CHECK(env.lookupOwn("many_150")->asInt() == -150);
  CHECK(env.lookupOwn("many_missing") == nullptr);
}

TEST_CASE("variables defined before and after the index is built are both found") {
  Environment env;
  for (int i = 0; i < 8; ++i) env.define("early_" + std::to_string(i), Value::integer(i));
  env.define("late_a", Value::integer(100));  // the ninth variable switches to indexed lookup
  env.define("late_b", Value::integer(101));
  for (int i = 0; i < 8; ++i) CHECK(env.lookupOwn("early_" + std::to_string(i))->asInt() == i);
  CHECK(env.lookupOwn("late_a")->asInt() == 100);
  CHECK(env.lookupOwn("late_b")->asInt() == 101);
}
