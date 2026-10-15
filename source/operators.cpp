#include <mni/interpreter.h>

#include <cmath>

namespace mni {

  namespace {
    // Integer results that would not fit in an int64 fall back to doubles.
    constexpr double kIntLimit = 9.2e18;

    Value intPow(int64_t base, int64_t exp) {
      if (exp < 0)
        return Value::real(std::pow(static_cast<double>(base), static_cast<double>(exp)));
      int64_t result = 1;
      for (int64_t i = 0; i < exp; ++i) {
        if (std::fabs(static_cast<double>(result) * static_cast<double>(base)) >= kIntLimit)
          return Value::real(std::pow(static_cast<double>(base), static_cast<double>(exp)));
        result *= base;
      }
      return Value::integer(result);
    }
  }  // namespace

  Value Interpreter::binaryOp(TokenType op, const Value& l, const Value& r, int line) {
    using T = Value::Type;
    auto mismatch = [&]() -> NuruError {
      return NuruError(line, std::string("Operesheni haijulikani: ") + l.typeName() + " "
                                 + std::string(tokenTypeName(op)) + " " + r.typeName());
    };

    switch (op) {
      case TokenType::Eq:
        return Value::boolean(valuesEqual(l, r));
      case TokenType::NotEq:
        return Value::boolean(!valuesEqual(l, r));
      case TokenType::In:
        switch (r.type()) {
          case T::Array:
            for (const Value& item : r.asArray().items)
              if (valuesEqual(l, item)) return Value::boolean(true);
            return Value::boolean(false);
          case T::Dict:
            return Value::boolean(r.asDict().find(l) != nullptr);
          case T::String:
            if (l.type() == T::String)
              return Value::boolean(r.asString().find(l.asString()) != std::string::npos);
            throw mismatch();
          default:
            throw mismatch();
        }
      default:
        break;
    }

    if (op == TokenType::Lt || op == TokenType::Lte || op == TokenType::Gt
        || op == TokenType::Gte) {
      int cmp;
      if (l.isNumber() && r.isNumber()) {
        if (l.type() == T::Int && r.type() == T::Int) {
          cmp = l.asInt() < r.asInt() ? -1 : l.asInt() > r.asInt() ? 1 : 0;
        } else {
          double a = l.toDouble(), b = r.toDouble();
          cmp = a < b ? -1 : a > b ? 1 : 0;
          if (std::isnan(a) || std::isnan(b)) return Value::boolean(false);
        }
      } else if (l.type() == T::String && r.type() == T::String) {
        int c = l.asString().compare(r.asString());
        cmp = c < 0 ? -1 : c > 0 ? 1 : 0;
      } else {
        throw mismatch();
      }
      switch (op) {
        case TokenType::Lt:
          return Value::boolean(cmp < 0);
        case TokenType::Lte:
          return Value::boolean(cmp <= 0);
        case TokenType::Gt:
          return Value::boolean(cmp > 0);
        default:
          return Value::boolean(cmp >= 0);
      }
    }

    if (op == TokenType::Plus) {
      if (l.type() == T::String && r.type() == T::String)
        return Value::string(l.asString() + r.asString());
      if (l.type() == T::Array && r.type() == T::Array) {
        std::vector<Value> items = l.asArray().items;
        items.insert(items.end(), r.asArray().items.begin(), r.asArray().items.end());
        return Value::array(std::move(items));
      }
      if (l.type() == T::Dict && r.type() == T::Dict) {
        Value merged = Value::dict();
        for (const auto& [k, v] : l.asDict().entries) merged.asDict().set(k, v);
        for (const auto& [k, v] : r.asDict().entries) merged.asDict().set(k, v);
        return merged;
      }
    }
    if (op == TokenType::Asterisk) {
      const Value* str = l.type() == T::String ? &l : r.type() == T::String ? &r : nullptr;
      const Value* count = l.type() == T::Int ? &l : r.type() == T::Int ? &r : nullptr;
      if (str && count) {
        if (count->asInt() < 0) throw NuruError(line, "Huwezi kurudia neno mara hasi");
        std::string out;
        for (int64_t i = 0; i < count->asInt(); ++i) out += str->asString();
        return Value::string(std::move(out));
      }
    }

    if (!l.isNumber() || !r.isNumber()) throw mismatch();

    if (l.type() == T::Int && r.type() == T::Int) {
      const int64_t a = l.asInt(), b = r.asInt();
      const double approx = [&] {
        switch (op) {
          case TokenType::Plus:
            return static_cast<double>(a) + static_cast<double>(b);
          case TokenType::Minus:
            return static_cast<double>(a) - static_cast<double>(b);
          default:
            return static_cast<double>(a) * static_cast<double>(b);
        }
      }();
      switch (op) {
        case TokenType::Plus:
          if (std::fabs(approx) >= kIntLimit) return Value::real(approx);
          return Value::integer(a + b);
        case TokenType::Minus:
          if (std::fabs(approx) >= kIntLimit) return Value::real(approx);
          return Value::integer(a - b);
        case TokenType::Asterisk:
          if (std::fabs(approx) >= kIntLimit) return Value::real(approx);
          return Value::integer(a * b);
        case TokenType::Slash:
          if (b == 0) throw NuruError(line, "Huwezi kugawanya kwa sifuri");
          if (b == -1 || a % b != 0)
            return Value::real(static_cast<double>(a) / static_cast<double>(b));
          return Value::integer(a / b);
        case TokenType::Modulus:
          if (b == 0) throw NuruError(line, "Huwezi kugawanya kwa sifuri");
          if (b == -1) return Value::integer(0);
          return Value::integer(a % b);
        case TokenType::Pow:
          return intPow(a, b);
        default:
          throw mismatch();
      }
    }

    const double a = l.toDouble(), b = r.toDouble();
    switch (op) {
      case TokenType::Plus:
        return Value::real(a + b);
      case TokenType::Minus:
        return Value::real(a - b);
      case TokenType::Asterisk:
        return Value::real(a * b);
      case TokenType::Slash:
        if (b == 0) throw NuruError(line, "Huwezi kugawanya kwa sifuri");
        return Value::real(a / b);
      case TokenType::Modulus:
        if (b == 0) throw NuruError(line, "Huwezi kugawanya kwa sifuri");
        return Value::real(std::fmod(a, b));
      case TokenType::Pow:
        return Value::real(std::pow(a, b));
      default:
        throw mismatch();
    }
  }

  Value Interpreter::indexValue(const Value& object, const Value& index, int line) {
    using T = Value::Type;
    switch (object.type()) {
      case T::Array: {
        if (index.type() != T::Int)
          throw NuruError(line,
                          std::string("Index ya orodha lazima iwe NAMBA, si ") + index.typeName());
        const auto& items = object.asArray().items;
        int64_t i = index.asInt();
        if (i < 0) i += static_cast<int64_t>(items.size());
        if (i < 0 || i >= static_cast<int64_t>(items.size())) return Value();
        return items[static_cast<size_t>(i)];
      }
      case T::String: {
        if (index.type() != T::Int)
          throw NuruError(line,
                          std::string("Index ya neno lazima iwe NAMBA, si ") + index.typeName());
        std::vector<std::string> chars = utf8Chars(object.asString());
        int64_t i = index.asInt();
        if (i < 0) i += static_cast<int64_t>(chars.size());
        if (i < 0 || i >= static_cast<int64_t>(chars.size())) return Value();
        return Value::string(std::move(chars[static_cast<size_t>(i)]));
      }
      case T::Dict: {
        if (!Dict::hashable(index))
          throw NuruError(line, std::string("Ufunguo wa kamusi hauwezi kuwa ") + index.typeName());
        Value* v = object.asDict().find(index);
        return v ? *v : Value();
      }
      default:
        throw NuruError(line, std::string("Huwezi kutumia index kwenye ") + object.typeName());
    }
  }

  void Interpreter::setIndex(const Value& object, const Value& index, Value value, int line) {
    using T = Value::Type;
    switch (object.type()) {
      case T::Array: {
        if (index.type() != T::Int)
          throw NuruError(line,
                          std::string("Index ya orodha lazima iwe NAMBA, si ") + index.typeName());
        auto& items = object.asArray().items;
        int64_t i = index.asInt();
        if (i < 0) i += static_cast<int64_t>(items.size());
        if (i < 0 || i >= static_cast<int64_t>(items.size()))
          throw NuruError(line, "Index nje ya mipaka: " + std::to_string(index.asInt()));
        items[static_cast<size_t>(i)] = std::move(value);
        return;
      }
      case T::Dict:
        if (!Dict::hashable(index))
          throw NuruError(line, std::string("Ufunguo wa kamusi hauwezi kuwa ") + index.typeName());
        object.asDict().set(index, std::move(value));
        return;
      case T::String:
        throw NuruError(line, "Maneno hayabadilishwi (NENO ni lisilobadilika)");
      default:
        throw NuruError(line, std::string("Huwezi kuweka kwa index kwenye ") + object.typeName());
    }
  }

}  // namespace mni
