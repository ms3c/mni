#include <mni/ast.h>
#include <mni/environment.h>
#include <mni/value.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace mni {

  Value Value::boolean(bool b) { return Value(Data(b)); }
  Value Value::integer(int64_t i) { return Value(Data(i)); }
  Value Value::real(double d) { return Value(Data(d)); }
  Value Value::string(std::string s) {
    return Value(Data(std::make_shared<const std::string>(std::move(s))));
  }
  Value Value::array(std::vector<Value> items) {
    auto a = std::make_shared<Array>();
    a->items = std::move(items);
    return Value(Data(std::move(a)));
  }
  Value Value::dict() { return Value(Data(std::make_shared<Dict>())); }
  Value Value::function(std::shared_ptr<Function> f) { return Value(Data(std::move(f))); }
  Value Value::builtin(std::shared_ptr<Builtin> b) { return Value(Data(std::move(b))); }
  Value Value::package(std::shared_ptr<Package> p) { return Value(Data(std::move(p))); }
  Value Value::native(std::string name, BuiltinFn fn, bool acceptsNamed) {
    return builtin(
        std::make_shared<Builtin>(Builtin{std::move(name), std::move(fn), acceptsNamed}));
  }

  bool Value::isTruthy() const { return !(isNull() || (type() == Type::Bool && !asBool())); }

  double Value::toDouble() const {
    return type() == Type::Int ? static_cast<double>(asInt()) : asFloat();
  }

  const char* Value::typeName() const {
    switch (type()) {
      case Type::Null:
        return "TUPU";
      case Type::Bool:
        return "BOOLEAN";
      case Type::Int:
        return "NAMBA";
      case Type::Float:
        return "DESIMALI";
      case Type::String:
        return "NENO";
      case Type::Array:
        return "ORODHA";
      case Type::Dict:
        return "KAMUSI";
      case Type::Function:
        return "KAZI";
      case Type::Builtin:
        return "KAZI_YA_NDANI";
      case Type::Package:
        return asPackage().typeName.c_str();
    }
    return "?";
  }

  bool Value::sameObject(const Value& other) const {
    if (type() != other.type()) return false;
    switch (type()) {
      case Type::Array:
        return &asArray() == &other.asArray();
      case Type::Dict:
        return &asDict() == &other.asDict();
      case Type::Function:
        return &asFunction() == &other.asFunction();
      case Type::Builtin:
        return &asBuiltin() == &other.asBuiltin();
      case Type::Package:
        return &asPackage() == &other.asPackage();
      default:
        return false;
    }
  }

  std::string formatFloat(double d) {
    if (std::isnan(d)) return "nan";
    if (std::isinf(d)) return d < 0 ? "-inf" : "inf";

    // Shortest digits that read back as the same double, from "%.{p}e".
    char buf[48];
    for (int precision = 0; precision <= 16; ++precision) {
      std::snprintf(buf, sizeof buf, "%.*e", precision, d);
      if (std::strtod(buf, nullptr) == d) break;
    }
    std::string text = buf;
    const bool negative = text[0] == '-';
    if (negative) text.erase(0, 1);
    const size_t e = text.find('e');
    const int exponent = std::atoi(text.c_str() + e + 1);
    std::string digits;
    for (size_t i = 0; i < e; ++i)
      if (text[i] != '.') digits += text[i];

    std::string out;
    if (exponent >= -7 && exponent < 21) {  // plain notation, like JavaScript
      if (exponent >= 0) {
        const size_t intLen = static_cast<size_t>(exponent) + 1;
        if (digits.size() <= intLen) {
          out = digits + std::string(intLen - digits.size(), '0');
        } else {
          out = digits.substr(0, intLen) + "." + digits.substr(intLen);
        }
      } else {
        out = "0." + std::string(static_cast<size_t>(-exponent - 1), '0') + digits;
      }
    } else {
      out = digits.substr(0, 1);
      if (digits.size() > 1) out += "." + digits.substr(1);
      out += std::string("e") + (exponent < 0 ? "-" : "+") + std::to_string(std::abs(exponent));
    }
    return negative ? "-" + out : out;
  }

  void Value::write(std::string& out, bool quoteStrings, std::vector<const void*>& path) const {
    switch (type()) {
      case Type::Null:
        out += "tupu";
        return;
      case Type::Bool:
        out += asBool() ? "kweli" : "sikweli";
        return;
      case Type::Int:
        out += std::to_string(asInt());
        return;
      case Type::Float:
        out += formatFloat(asFloat());
        return;
      case Type::String:
        if (!quoteStrings) {
          out += asString();
          return;
        }
        out += '"';
        for (char c : asString()) {
          switch (c) {
            case '"':
              out += "\\\"";
              break;
            case '\\':
              out += "\\\\";
              break;
            case '\n':
              out += "\\n";
              break;
            case '\t':
              out += "\\t";
              break;
            default:
              out += c;
          }
        }
        out += '"';
        return;
      case Type::Array: {
        const void* self = &asArray();
        if (std::find(path.begin(), path.end(), self) != path.end()) {
          out += "[...]";
          return;
        }
        path.push_back(self);
        out += '[';
        bool first = true;
        for (const Value& v : asArray().items) {
          if (!first) out += ", ";
          first = false;
          v.write(out, true, path);
        }
        out += ']';
        path.pop_back();
        return;
      }
      case Type::Dict: {
        const void* self = &asDict();
        if (std::find(path.begin(), path.end(), self) != path.end()) {
          out += "{...}";
          return;
        }
        path.push_back(self);
        out += '{';
        bool first = true;
        for (const auto& [k, v] : asDict().entries) {
          if (!first) out += ", ";
          first = false;
          k.write(out, true, path);
          out += ": ";
          v.write(out, true, path);
        }
        out += '}';
        path.pop_back();
        return;
      }
      case Type::Function: {
        out += "unda";
        const FunctionLiteral* fn = asFunction().node;
        if (!fn->name.empty()) out += " " + fn->name;
        out += "(";
        for (size_t i = 0; i < fn->parameters.size(); ++i) {
          if (i) out += ", ";
          out += fn->parameters[i].name;
        }
        out += ") {...}";
        return;
      }
      case Type::Builtin:
        out += "<kazi " + asBuiltin().name + ">";
        return;
      case Type::Package: {
        const Package& p = asPackage();
        out += p.text.empty() ? "<" + p.typeName + " " + p.name + ">" : p.text;
        return;
      }
    }
  }

  std::string Value::display() const {
    std::string out;
    std::vector<const void*> path;
    write(out, false, path);
    return out;
  }

  std::string Value::inspect() const {
    std::string out;
    std::vector<const void*> path;
    write(out, true, path);
    return out;
  }

  namespace {
    std::string keyOf(const Value& key) {
      switch (key.type()) {
        case Value::Type::Bool:
          return key.asBool() ? "b1" : "b0";
        case Value::Type::Int:
          return "i" + std::to_string(key.asInt());
        case Value::Type::Float:
          return "f" + formatFloat(key.asFloat());
        default:
          return "s" + key.asString();
      }
    }
  }  // namespace

  bool Dict::hashable(const Value& key) {
    switch (key.type()) {
      case Value::Type::Bool:
      case Value::Type::Int:
      case Value::Type::Float:
      case Value::Type::String:
        return true;
      default:
        return false;
    }
  }

  Value* Dict::find(const Value& key) {
    if (!hashable(key)) return nullptr;
    auto it = index_.find(keyOf(key));
    return it == index_.end() ? nullptr : &entries[it->second].second;
  }

  void Dict::set(const Value& key, Value value) {
    auto [it, inserted] = index_.try_emplace(keyOf(key), entries.size());
    if (inserted) {
      entries.emplace_back(key, std::move(value));
    } else {
      entries[it->second].second = std::move(value);
    }
  }

  bool valuesEqual(const Value& a, const Value& b) {
    if (a.isNumber() && b.isNumber()) {
      if (a.type() == Value::Type::Int && b.type() == Value::Type::Int)
        return a.asInt() == b.asInt();
      return a.toDouble() == b.toDouble();
    }
    if (a.type() != b.type()) return false;
    switch (a.type()) {
      case Value::Type::Null:
        return true;
      case Value::Type::Bool:
        return a.asBool() == b.asBool();
      case Value::Type::String:
        return a.asString() == b.asString();
      case Value::Type::Array: {
        const auto& x = a.asArray().items;
        const auto& y = b.asArray().items;
        if (x.size() != y.size()) return false;
        for (size_t i = 0; i < x.size(); ++i)
          if (!valuesEqual(x[i], y[i])) return false;
        return true;
      }
      case Value::Type::Dict: {
        auto& x = a.asDict();
        auto& y = b.asDict();
        if (x.entries.size() != y.entries.size()) return false;
        for (const auto& [k, v] : x.entries) {
          Value* other = y.find(k);
          if (!other || !valuesEqual(v, *other)) return false;
        }
        return true;
      }
      default:
        return a.sameObject(b);
    }
  }

  std::vector<std::string> utf8Chars(const std::string& s) {
    std::vector<std::string> out;
    for (size_t i = 0; i < s.size();) {
      auto c = static_cast<unsigned char>(s[i]);
      size_t len = c < 0x80           ? 1
                   : (c >> 5) == 0x6  ? 2
                   : (c >> 4) == 0xE  ? 3
                   : (c >> 3) == 0x1E ? 4
                                      : 1;
      if (i + len > s.size()) len = 1;
      out.emplace_back(s.substr(i, len));
      i += len;
    }
    return out;
  }

}  // namespace mni
