#include <mni/interpreter.h>

#include <cctype>
#include <chrono>
#include <ctime>
#include <thread>

#include "native.h"

namespace mni::native {

  namespace {
    constexpr const char* kTimeFormat = "%H:%M:%S %d-%m-%Y";

    bool localTime(int64_t epoch, std::tm& out) {
      std::time_t t = static_cast<std::time_t>(epoch);
#ifdef _WIN32
      return localtime_s(&out, &t) == 0;
#else
      return localtime_r(&t, &out) != nullptr;
#endif
    }

    std::string formatEpoch(int64_t epoch, const char* format) {
      std::tm tm{};
      if (!localTime(epoch, tm)) return "";
      char buf[64];
      return std::strftime(buf, sizeof buf, format, &tm) ? buf : "";
    }

    // Reads "HH:mm:ss dd-MM-YYYY" into its six numbers.
    bool parseTimeText(const std::string& text, int (&fields)[6]) {
      const char separators[6] = {':', ':', ' ', '-', '-', '\0'};
      size_t i = 0;
      for (int field = 0; field < 6; ++field) {
        const size_t start = i;
        while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) ++i;
        if (i == start || i - start > 9) return false;
        fields[field] = std::stoi(text.substr(start, i - start));
        if (separators[field] != '\0') {
          if (i >= text.size() || text[i] != separators[field]) return false;
          ++i;
        }
      }
      return i == text.size();
    }

    int64_t nowEpoch() { return static_cast<int64_t>(std::time(nullptr)); }

    Value makeMuda(int64_t epoch) {
      Value obj = makePackage("muda", "MUDA", formatEpoch(epoch, kTimeFormat));
      Environment& env = *obj.asPackage().env;
      env.define("__epoch", Value::integer(epoch));
      addFunction(
          obj, "ongeza",
          [epoch](Interpreter&, std::vector<Value>& args, NamedArgs& named, int line) {
            if (!args.empty() || named.empty())
              throw NuruError(line,
                              "'ongeza' inahitaji hoja zenye majina: sekunde, dakika, masaa, siku, "
                              "wiki, miezi, miaka");
            std::tm tm{};
            if (!localTime(epoch, tm)) throw NuruError(line, "'ongeza': muda si sahihi");
            for (auto& [key, value] : named) {
              const int amount = static_cast<int>(numberArg(key, value, line));
              if (key == "sekunde")
                tm.tm_sec += amount;
              else if (key == "dakika")
                tm.tm_min += amount;
              else if (key == "masaa")
                tm.tm_hour += amount;
              else if (key == "siku")
                tm.tm_mday += amount;
              else if (key == "wiki")
                tm.tm_mday += 7 * amount;
              else if (key == "miezi")
                tm.tm_mon += amount;
              else if (key == "miaka")
                tm.tm_year += amount;
              else
                throw NuruError(line, "'ongeza' haina kigezo '" + key + "'");
            }
            tm.tm_isdst = -1;
            std::time_t result = std::mktime(&tm);
            if (result == static_cast<std::time_t>(-1))
              throw NuruError(line, "'ongeza': tarehe si sahihi");
            return makeMuda(static_cast<int64_t>(result));
          },
          true);
      return obj;
    }

    // A muda object, or a string like "15:00:00 01-01-2024".
    int64_t epochOf(const std::string& fn, const Value& v, int line) {
      if (v.type() == Value::Type::Package && v.asPackage().typeName == "MUDA")
        return v.asPackage().env->lookupOwn("__epoch")->asInt();
      if (v.type() == Value::Type::String) {
        int f[6];
        if (!parseTimeText(v.asString(), f))
          throw NuruError(line, "'" + fn + "': muda unatakiwa kuwa 'HH:mm:ss dd-MM-YYYY'");
        std::tm tm{};
        tm.tm_hour = f[0];
        tm.tm_min = f[1];
        tm.tm_sec = f[2];
        tm.tm_mday = f[3];
        tm.tm_mon = f[4] - 1;
        tm.tm_year = f[5] - 1900;
        tm.tm_isdst = -1;
        std::time_t t = std::mktime(&tm);
        if (t == static_cast<std::time_t>(-1))
          throw NuruError(line, "'" + fn + "': tarehe si sahihi");
        return static_cast<int64_t>(t);
      }
      throw NuruError(line, "'" + fn + "' inahitaji muda au NENO, si " + v.typeName());
    }
  }  // namespace

  Value createMuda() {
    Value pkg = makePackage("muda", "PAKEJI");
    addFunction(pkg, "hasahivi", [](Interpreter&, std::vector<Value>& args, NamedArgs&, int line) {
      expectArgs("hasahivi", args, 0, 0, line);
      return makeMuda(nowEpoch());
    });
    addFunction(pkg, "leo", [](Interpreter&, std::vector<Value>& args, NamedArgs&, int line) {
      expectArgs("leo", args, 0, 0, line);
      return Value::string(formatEpoch(nowEpoch(), "%d-%m-%Y"));
    });
    addFunction(pkg, "tangu", [](Interpreter&, std::vector<Value>& args, NamedArgs&, int line) {
      expectArgs("tangu", args, 1, 1, line);
      return Value::integer(nowEpoch() - epochOf("tangu", args[0], line));
    });
    addFunction(pkg, "lala", [](Interpreter&, std::vector<Value>& args, NamedArgs&, int line) {
      expectArgs("lala", args, 1, 1, line);
      double seconds = numberArg("lala", args[0], line);
      if (seconds < 0) throw NuruError(line, "'lala' haiwezi kuchukua sekunde hasi");
      std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
      return Value();
    });
    addFunction(pkg, "baada_ya", [](Interpreter&, std::vector<Value>& args, NamedArgs&, int line) {
      expectArgs("baada_ya", args, 1, 1, line);
      return makeMuda(nowEpoch() + static_cast<int64_t>(numberArg("baada_ya", args[0], line)));
    });
    addFunction(pkg, "tofauti", [](Interpreter&, std::vector<Value>& args, NamedArgs&, int line) {
      expectArgs("tofauti", args, 2, 2, line);
      return Value::integer(epochOf("tofauti", args[0], line) - epochOf("tofauti", args[1], line));
    });
    return pkg;
  }

}  // namespace mni::native
