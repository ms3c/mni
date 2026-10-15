#pragma once

// Internal: helpers shared by the built-in functions and modules. Not installed.

#include <mni/interpreter.h>

#include <string>
#include <vector>

namespace mni {

  /** Creates a built-in module (`hisabati`, `jsoni`, `muda`), or returns tupu for other names. */
  Value createNativeModule(const std::string& name);

  /** Modules that exist in Nuru but are not implemented here yet (`mtandao`, `os`). */
  bool isUnimplementedModule(const std::string& name);

  /** Implements `fungua`: a FAILI object whose `soma()` returns the file contents. */
  Value openFile(const std::string& path, int line);

}  // namespace mni

namespace mni::native {

  Value createHisabati();
  Value createJsoni();
  Value createMuda();

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

  inline const std::string& stringArg(const std::string& name, const Value& v, int line) {
    if (v.type() != Value::Type::String)
      throw NuruError(line, "'" + name + "' inahitaji NENO, si " + v.typeName());
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
