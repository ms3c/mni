#include <mni/interpreter.h>

#include <cmath>
#include <cstdlib>
#include <random>

#include "native.h"

namespace mni::native {

  namespace {
    constexpr double kIntLimit = 9.2e18;

    // An integral double as a NAMBA when it fits, otherwise as a DESIMALI.
    Value integral(double v) {
      if (std::isfinite(v) && std::fabs(v) < kIntLimit)
        return Value::integer(static_cast<int64_t>(v));
      return Value::real(v);
    }

    // Numbers given either as one array or as separate arguments.
    std::vector<Value> numberList(const std::string& name, const std::vector<Value>& args,
                                  int line) {
      std::vector<Value> items;
      if (args.size() == 1 && args[0].type() == Value::Type::Array) {
        items = args[0].asArray().items;
      } else {
        items = args;
      }
      if (items.empty()) throw NuruError(line, "'" + name + "' inahitaji angalau namba moja");
      for (const Value& v : items) numberArg(name, v, line);
      return items;
    }
  }  // namespace

  Value createHisabati() {
    Value pkg = makePackage("hisabati", "PAKEJI");
    Environment& env = *pkg.asPackage().env;

    env.define("PI", Value::real(3.14159265358979323846));
    env.define("e", Value::real(2.71828182845904523536));
    env.define("phi", Value::real(1.61803398874989484820));
    env.define("ln10", Value::real(2.30258509299404568402));
    env.define("ln2", Value::real(0.693147180559945309417));
    env.define("log10e", Value::real(0.434294481903251827651));
    env.define("log2e", Value::real(1.44269504088896340736));
    env.define("sqrt1_2", Value::real(0.707106781186547524401));
    env.define("sqrt2", Value::real(1.41421356237309504880));
    env.define("sqrt3", Value::real(1.73205080756887729353));
    env.define("sqrt5", Value::real(2.23606797749978969641));
    env.define("EPSILON", Value::real(2.220446049250313e-16));

    using Unary = double (*)(double);
    const std::pair<const char*, Unary> unary[] = {
        {"cos", [](double x) { return std::cos(x); }},
        {"sin", [](double x) { return std::sin(x); }},
        {"tan", [](double x) { return std::tan(x); }},
        {"acos", [](double x) { return std::acos(x); }},
        {"asin", [](double x) { return std::asin(x); }},
        {"atan", [](double x) { return std::atan(x); }},
        {"cosh", [](double x) { return std::cosh(x); }},
        {"sinh", [](double x) { return std::sinh(x); }},
        {"tanh", [](double x) { return std::tanh(x); }},
        {"acosh", [](double x) { return std::acosh(x); }},
        {"asinh", [](double x) { return std::asinh(x); }},
        {"atanh", [](double x) { return std::atanh(x); }},
        {"exp", [](double x) { return std::exp(x); }},
        {"expm1", [](double x) { return std::expm1(x); }},
        {"log", [](double x) { return std::log(x); }},
        {"log2", [](double x) { return std::log2(x); }},
        {"log10", [](double x) { return std::log10(x); }},
        {"log1p", [](double x) { return std::log1p(x); }},
        {"sqrt", [](double x) { return std::sqrt(x); }},
        {"cbrt", [](double x) { return std::cbrt(x); }},
    };
    for (const auto& entry : unary) {
      const std::string n = entry.first;
      const Unary f = entry.second;
      addFunction(pkg, n, [n, f](Interpreter&, std::vector<Value>& args, NamedArgs&, int line) {
        expectArgs(n, args, 1, 1, line);
        return Value::real(f(numberArg(n, args[0], line)));
      });
    }

    addFunction(pkg, "abs", [](Interpreter&, std::vector<Value>& args, NamedArgs&, int line) {
      expectArgs("abs", args, 1, 1, line);
      numberArg("abs", args[0], line);
      if (args[0].type() == Value::Type::Int) return Value::integer(std::llabs(args[0].asInt()));
      return Value::real(std::fabs(args[0].asFloat()));
    });

    addFunction(pkg, "sign", [](Interpreter&, std::vector<Value>& args, NamedArgs&, int line) {
      expectArgs("sign", args, 1, 1, line);
      double x = numberArg("sign", args[0], line);
      return Value::integer(x > 0 ? 1 : x < 0 ? -1 : 0);
    });

    using Rounder = double (*)(double);
    const std::pair<const char*, Rounder> rounders[] = {
        {"ceil", [](double x) { return std::ceil(x); }},
        {"floor", [](double x) { return std::floor(x); }},
    };
    for (const auto& entry : rounders) {
      const std::string n = entry.first;
      const Rounder f = entry.second;
      addFunction(pkg, n, [n, f](Interpreter&, std::vector<Value>& args, NamedArgs&, int line) {
        expectArgs(n, args, 1, 1, line);
        return integral(f(numberArg(n, args[0], line)));
      });
    }

    addFunction(pkg, "round", [](Interpreter&, std::vector<Value>& args, NamedArgs&, int line) {
      expectArgs("round", args, 1, 1,
                 line);  // the optional rounding-method argument is not supported yet
      return integral(std::round(numberArg("round", args[0], line)));
    });

    addFunction(pkg, "atan2", [](Interpreter&, std::vector<Value>& args, NamedArgs&, int line) {
      expectArgs("atan2", args, 2, 2, line);
      return Value::real(
          std::atan2(numberArg("atan2", args[0], line), numberArg("atan2", args[1], line)));
    });

    addFunction(pkg, "hypot", [](Interpreter&, std::vector<Value>& args, NamedArgs&, int line) {
      double sum = 0;
      for (const Value& v : numberList("hypot", args, line)) sum += v.toDouble() * v.toDouble();
      return Value::real(std::sqrt(sum));
    });

    addFunction(pkg, "root", [](Interpreter&, std::vector<Value>& args, NamedArgs&, int line) {
      expectArgs("root", args, 2, 2, line);
      double x = numberArg("root", args[0], line), n = numberArg("root", args[1], line);
      if (n == 0) throw NuruError(line, "'root' haiwezi kuchukua kiwango sifuri");
      bool negative = x < 0;
      if (negative && std::fmod(n, 2) == 0) return Value::real(std::nan(""));
      double r = std::pow(std::fabs(x), 1.0 / n);
      double rounded = std::round(r);
      if (std::pow(rounded, n) == std::fabs(x))
        r = rounded;  // exact roots stay exact: root(27, 3) == 3
      return Value::real(negative ? -r : r);
    });

    addFunction(pkg, "factorial", [](Interpreter&, std::vector<Value>& args, NamedArgs&, int line) {
      expectArgs("factorial", args, 1, 1, line);
      if (args[0].type() != Value::Type::Int || args[0].asInt() < 0)
        throw NuruError(line, "'factorial' inahitaji NAMBA isiyo hasi");
      int64_t n = args[0].asInt();
      if (n > 20) return Value::real(std::tgamma(static_cast<double>(n) + 1));  // beyond int64
      int64_t result = 1;
      for (int64_t i = 2; i <= n; ++i) result *= i;
      return Value::integer(result);
    });

    // Not in the written docs, but upstream's examples use it: a float in [0, 1).
    addFunction(pkg, "random", [](Interpreter&, std::vector<Value>& args, NamedArgs&, int line) {
      expectArgs("random", args, 0, 0, line);
      static std::mt19937_64 engine{std::random_device{}()};
      return Value::real(std::uniform_real_distribution<double>(0.0, 1.0)(engine));
    });

    for (const char* which : {"max", "min"}) {
      const std::string n = which;
      addFunction(pkg, n, [n](Interpreter&, std::vector<Value>& args, NamedArgs&, int line) {
        std::vector<Value> items = numberList(n, args, line);
        Value best = items[0];
        for (const Value& v : items)
          if (n == "max" ? v.toDouble() > best.toDouble() : v.toDouble() < best.toDouble())
            best = v;
        return best;
      });
    }
    return pkg;
  }

}  // namespace mni::native
