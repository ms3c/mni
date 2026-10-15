#pragma once

// Internal: helpers shared by the built-in functions and modules. Not installed.

#include <mni/interpreter.h>

#include <string>
#include <string_view>
#include <vector>

namespace mni {

  /** Creates a built-in module (`hisabati`, `jsoni`, `muda`, `os`, `mtandao`), or returns tupu. */
  Value createNativeModule(const std::string& name);

  /** Modules this build leaves out: `mtandao` when configured with -DMNI_ENABLE_HTTP=OFF. */
  bool isUnavailableModule(const std::string& name);

  /** Implements `fungua`: a FAILI object whose `soma()` returns the file contents. */
  Value openFile(const std::string& path, int line);

}  // namespace mni

namespace mni::native {

  Value createHisabati();
  Value createJsoni();
  Value createMuda();
  Value createOs();
  Value createMtandao();  // tupu when built without HTTP support

  /** Compact JSON text for a value (what jsoni.enkodi produces). */
  std::string jsonEncode(const Value& value, int line);

  inline void addFunction(const Value& pkg, const std::string& name, BuiltinFn fn,
                          bool acceptsNamed = false) {
    pkg.asPackage().env->define(name, Value::native(name, std::move(fn), acceptsNamed));
  }

  inline void expectArgs(const std::string& name, const std::vector<Value>& args, size_t min,
                         size_t max, int line) {
    if (args.size() >= min && args.size() <= max) return;
    std::string expected
        = min == max ? std::to_string(min) : std::to_string(min) + " hadi " + std::to_string(max);
    throw NuruError(line, "Idadi ya hoja si sahihi kwa '" + name + "': tulitegemea " + expected
                              + ", tumepata " + std::to_string(args.size()));
  }

  inline int64_t intArg(const std::string& name, const Value& v, int line) {
    if (v.type() != Value::Type::Int)
      throw NuruError(line, "'" + name + "' inahitaji NAMBA, si " + v.typeName());
    return v.asInt();
  }

  inline double numberArg(const std::string& name, const Value& v, int line) {
    if (!v.isNumber()) throw NuruError(line, "'" + name + "' inahitaji namba, si " + v.typeName());
    return v.toDouble();
  }

  // `name` is a string_view so callers can pass a literal: with a `const std::string&` parameter
  // the literal becomes a temporary and GCC warns that the returned reference might dangle.
  inline const std::string& stringArg(std::string_view name, const Value& v, int line) {
    if (v.type() != Value::Type::String)
      throw NuruError(line, "'" + std::string(name) + "' inahitaji NENO, si " + v.typeName());
    return v.asString();
  }

  /** A package-like object: a named scope whose members are its variables. */
  inline Value makePackage(const std::string& name, const std::string& typeName,
                           const std::string& text = "") {
    auto pkg = std::make_shared<Package>();
    pkg->name = name;
    pkg->typeName = typeName;
    pkg->text = text;
    pkg->env = std::make_shared<Environment>();
    return Value::package(std::move(pkg));
  }

}  // namespace mni::native
