#include <cmath>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>

#include "native.h"

namespace mni::native {

  namespace {
    // ordered_json keeps object keys in source order, like our dictionaries.
    using Json = nlohmann::ordered_json;

    constexpr int kMaxDepth = 200;

    Value fromJson(const Json& json, int depth, int line) {
      if (depth > kMaxDepth) throw NuruError(line, "JSON imejaa viwango vingi mno");
      switch (json.type()) {
        case Json::value_t::boolean:
          return Value::boolean(json.get<bool>());
        case Json::value_t::number_integer:
          return Value::integer(json.get<std::int64_t>());
        case Json::value_t::number_unsigned: {
          const auto n = json.get<std::uint64_t>();
          if (n <= static_cast<std::uint64_t>(INT64_MAX))
            return Value::integer(static_cast<std::int64_t>(n));
          return Value::real(static_cast<double>(n));  // too large for NAMBA
        }
        case Json::value_t::number_float:
          return Value::real(json.get<double>());
        case Json::value_t::string:
          return Value::string(json.get<std::string>());
        case Json::value_t::array: {
          std::vector<Value> items;
          items.reserve(json.size());
          for (const Json& item : json) items.push_back(fromJson(item, depth + 1, line));
          return Value::array(std::move(items));
        }
        case Json::value_t::object: {
          Value dict = Value::dict();
          for (const auto& [key, value] : json.items())
            dict.asDict().set(Value::string(key), fromJson(value, depth + 1, line));
          return dict;
        }
        default:  // null (and the binary/discarded kinds, which parsing text never produces)
          return Value();
      }
    }

    Json toJson(const Value& value, int depth, int line) {
      if (depth > kMaxDepth)
        throw NuruError(line, "enkodi: data ina viwango vingi mno (au inajirudia)");
      switch (value.type()) {
        case Value::Type::Null:
          return nullptr;
        case Value::Type::Bool:
          return value.asBool();
        case Value::Type::Int:
          return value.asInt();
        case Value::Type::Float:
          if (!std::isfinite(value.asFloat()))
            throw NuruError(line, "enkodi: DESIMALI isiyo na kikomo haiwezi kuwa JSON");
          return value.asFloat();
        case Value::Type::String:
          return value.asString();
        case Value::Type::Array: {
          Json array = Json::array();
          for (const Value& item : value.asArray().items)
            array.push_back(toJson(item, depth + 1, line));
          return array;
        }
        case Value::Type::Dict: {
          Json object = Json::object();
          for (const auto& [key, item] : value.asDict().entries)
            object[key.display()] = toJson(item, depth + 1, line);
          return object;
        }
        default:
          throw NuruError(line, std::string("enkodi: ") + value.typeName() + " haiwezi kuwa JSON");
      }
    }
  }  // namespace

  std::string jsonEncode(const Value& value, int line) {
    // Bytes that are not valid UTF-8 become U+FFFD instead of aborting the whole encode.
    return toJson(value, 0, line).dump(-1, ' ', false, Json::error_handler_t::replace);
  }

  Value createJsoni() {
    Value pkg = makePackage("jsoni", "PAKEJI");
    addFunction(pkg, "dikodi", [](Interpreter&, std::vector<Value>& args, NamedArgs&, int line) {
      expectArgs("dikodi", args, 1, 1, line);
      const std::string& text = stringArg("dikodi", args[0], line);
      try {
        return fromJson(Json::parse(text), 0, line);
      } catch (const Json::exception& e) {
        throw NuruError(line, std::string("JSON si sahihi: ") + e.what());
      }
    });
    addFunction(pkg, "enkodi", [](Interpreter&, std::vector<Value>& args, NamedArgs&, int line) {
      expectArgs("enkodi", args, 1, 1, line);
      return Value::string(jsonEncode(args[0], line));
    });
    return pkg;
  }

}  // namespace mni::native
