#include <mni/interpreter.h>

#include <cctype>
#include <string_view>
#include <unordered_map>

#include "native.h"

namespace mni {

  namespace {
    using native::expectArgs;

    using Method = Value (*)(Interpreter&, const Value& self, std::vector<Value>& args, int line);
    using MethodTable = std::unordered_map<std::string_view, Method>;

    Value changeCase(const Value& self, int (*convert)(int)) {
      std::string out = self.asString();
      for (char& c : out) c = static_cast<char>(convert(static_cast<unsigned char>(c)));
      return Value::string(std::move(out));
    }

    std::vector<Value> splitOnWhitespace(const std::string& s) {
      std::vector<Value> parts;
      std::string word;
      for (char c : s) {
        if (std::isspace(static_cast<unsigned char>(c))) {
          if (!word.empty()) parts.push_back(Value::string(std::move(word)));
          word.clear();
        } else {
          word += c;
        }
      }
      if (!word.empty()) parts.push_back(Value::string(std::move(word)));
      return parts;
    }

    Value splitOn(const std::string& s, const std::string& separator) {
      std::vector<Value> parts;
      if (separator.empty()) {
        for (std::string& ch : utf8Chars(s)) parts.push_back(Value::string(std::move(ch)));
      } else {
        size_t start = 0;
        for (size_t pos; (pos = s.find(separator, start)) != std::string::npos;
             start = pos + separator.size())
          parts.push_back(Value::string(s.substr(start, pos - start)));
        parts.push_back(Value::string(s.substr(start)));
      }
      return Value::array(std::move(parts));
    }

    const MethodTable& stringMethods() {
      static const MethodTable table = {
          {"idadi",
           [](Interpreter&, const Value& self, std::vector<Value>& args, int line) {
             expectArgs("idadi", args, 0, 0, line);
             return Value::integer(static_cast<int64_t>(utf8Chars(self.asString()).size()));
           }},
          {"herufikubwa",
           [](Interpreter&, const Value& self, std::vector<Value>& args, int line) {
             expectArgs("herufikubwa", args, 0, 0, line);
             return changeCase(self, std::toupper);
           }},
          {"herufindogo",
           [](Interpreter&, const Value& self, std::vector<Value>& args, int line) {
             expectArgs("herufindogo", args, 0, 0, line);
             return changeCase(self, std::tolower);
           }},
          {"gawa",
           [](Interpreter&, const Value& self, std::vector<Value>& args, int line) {
             expectArgs("gawa", args, 0, 1, line);
             if (args.empty()) return Value::array(splitOnWhitespace(self.asString()));
             return splitOn(self.asString(), native::stringArg("gawa", args[0], line));
           }},
      };
      return table;
    }

    // Shared by map and chuja: calls `fn` on a copy of the items, since the callback may modify
    // the array.
    template <class Collect> Value mapItems(Interpreter& interp, const Value& self,
                                            std::vector<Value>& args, int line, const char* name,
                                            Collect collect) {
      expectArgs(name, args, 1, 1, line);
      const Value fn = args[0];
      if (fn.type() != Value::Type::Function && fn.type() != Value::Type::Builtin)
        throw NuruError(line, "'" + std::string(name) + "' inahitaji kazi, si " + fn.typeName());
      const std::vector<Value> snapshot = self.asArray().items;
      std::vector<Value> result;
      for (const Value& item : snapshot)
        collect(result, item, interp.callFunction(fn, {item}, line));
      return Value::array(std::move(result));
    }

    const MethodTable& arrayMethods() {
      static const MethodTable table = {
          {"idadi",
           [](Interpreter&, const Value& self, std::vector<Value>& args, int line) {
             expectArgs("idadi", args, 0, 0, line);
             return Value::integer(static_cast<int64_t>(self.asArray().items.size()));
           }},
          {"sukuma",
           [](Interpreter&, const Value& self, std::vector<Value>& args, int) {
             for (Value& v : args) self.asArray().items.push_back(std::move(v));
             return Value();
           }},
          {"yamwisho",
           [](Interpreter&, const Value& self, std::vector<Value>& args, int line) {
             expectArgs("yamwisho", args, 0, 0, line);
             const auto& items = self.asArray().items;
             return items.empty() ? Value() : items.back();
           }},
          {"map",
           [](Interpreter& interp, const Value& self, std::vector<Value>& args, int line) {
             return mapItems(
                 interp, self, args, line, "map",
                 [](auto& out, const Value&, Value mapped) { out.push_back(std::move(mapped)); });
           }},
          {"chuja",
           [](Interpreter& interp, const Value& self, std::vector<Value>& args, int line) {
             return mapItems(interp, self, args, line, "chuja",
                             [](auto& out, const Value& item, const Value& keep) {
                               if (keep.isTruthy()) out.push_back(item);
                             });
           }},
      };
      return table;
    }
  }  // namespace

  Value Interpreter::callMethod(const Value& self, const std::string& name,
                                std::vector<Value>& args, NamedArgs& named, int line) {
    if (self.type() == Value::Type::Package)
      return callFunction(getProperty(self, name, line), std::move(args), line, std::move(named));
    if (!named.empty()) throw NuruError(line, "'" + name + "' haipokei hoja zenye majina");

    const MethodTable* table = self.type() == Value::Type::String  ? &stringMethods()
                               : self.type() == Value::Type::Array ? &arrayMethods()
                                                                   : nullptr;
    if (table) {
      auto it = table->find(name);
      if (it != table->end()) return it->second(*this, self, args, line);
    }
    throw NuruError(line, "Method '" + name + "' haipo kwa " + std::string(self.typeName()));
  }

}  // namespace mni
