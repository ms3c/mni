#include <mni/interpreter.h>

#include "native.h"

namespace mni {

  namespace {
    using native::expectArgs;
    using native::intArg;

    Value rangeBuiltin(Interpreter&, std::vector<Value>& args, NamedArgs&, int line) {
      expectArgs("mfululizo", args, 1, 3, line);
      int64_t start = 0, end, step = 1;
      if (args.size() == 1) {
        end = intArg("mfululizo", args[0], line);
      } else {
        start = intArg("mfululizo", args[0], line);
        end = intArg("mfululizo", args[1], line);
        if (args.size() == 3) step = intArg("mfululizo", args[2], line);
      }
      if (step == 0) throw NuruError(line, "Hatua ya 'mfululizo' haiwezi kuwa sifuri");
      std::vector<Value> items;
      constexpr size_t kMax = 50'000'000;
      for (int64_t i = start; step > 0 ? i < end : i > end; i += step) {
        if (items.size() >= kMax) throw NuruError(line, "'mfululizo' ni mrefu mno");
        items.push_back(Value::integer(i));
      }
      return Value::array(std::move(items));
    }
  }  // namespace

  void Interpreter::defineBuiltin(const std::string& name, BuiltinFn fn, bool acceptsNamed) {
    globals_->define(name, Value::native(name, std::move(fn), acceptsNamed));
  }

  void Interpreter::installBuiltins() {
    defineBuiltin("andika", [](Interpreter& interp, std::vector<Value>& args, NamedArgs&, int) {
      std::string text;
      for (size_t i = 0; i < args.size(); ++i) {
        if (i) text += ' ';
        text += args[i].display();
      }
      interp.out() << text << '\n';
      return Value();
    });

    defineBuiltin("jaza", [](Interpreter& interp, std::vector<Value>& args, NamedArgs&, int line) {
      expectArgs("jaza", args, 0, 1, line);
      if (!args.empty()) interp.out() << args[0].display() << std::flush;
      std::string input;
      if (!std::getline(interp.in(), input)) return Value();
      if (!input.empty() && input.back() == '\r') input.pop_back();
      return Value::string(std::move(input));
    });

    defineBuiltin("aina", [](Interpreter&, std::vector<Value>& args, NamedArgs&, int line) {
      expectArgs("aina", args, 1, 1, line);
      return Value::string(args[0].typeName());
    });

    defineBuiltin("mfululizo", rangeBuiltin);

    defineBuiltin("fungua", [](Interpreter&, std::vector<Value>& args, NamedArgs&, int line) {
      expectArgs("fungua", args, 1, 1, line);
      return openFile(native::stringArg("fungua", args[0], line), line);
    });

    auto exitBuiltin = [](Interpreter&, std::vector<Value>& args, NamedArgs&, int line) -> Value {
      expectArgs("toka", args, 0, 1, line);
      throw ExitRequest{args.empty() ? 0 : static_cast<int>(intArg("toka", args[0], line))};
    };
    defineBuiltin("toka", exitBuiltin);
    defineBuiltin("exit", exitBuiltin);
  }

}  // namespace mni
