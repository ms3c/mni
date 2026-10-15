#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace mni {

  class Interpreter;
  class Environment;
  struct FunctionLiteral;
  struct Array;
  struct Dict;
  struct Function;
  struct Builtin;
  struct Package;

  class Value;
  /** Keyword arguments of a call (`f(a, name = value)`), in source order. */
  using NamedArgs = std::vector<std::pair<std::string, Value>>;
  using BuiltinFn = std::function<Value(Interpreter&, std::vector<Value>&, NamedArgs&, int line)>;

  /**
   * @brief A Nuru runtime value
   *
   * Null, booleans and numbers are stored inline. Strings are immutable and shared; arrays, dicts
   * and functions are shared by reference (copying a Value aliases the same array).
   */
  class Value {
  public:
    // Same order as the variant alternatives below.
    enum class Type { Null, Bool, Int, Float, String, Array, Dict, Function, Builtin, Package };

    Value() = default;
    static Value boolean(bool b);
    static Value integer(int64_t i);
    static Value real(double d);
    static Value string(std::string s);
    static Value array(std::vector<Value> items = {});
    static Value dict();
    static Value function(std::shared_ptr<Function> f);
    static Value builtin(std::shared_ptr<Builtin> b);
    static Value package(std::shared_ptr<Package> p);
    static Value native(std::string name, BuiltinFn fn, bool acceptsNamed = false);

    Type type() const { return static_cast<Type>(data_.index()); }
    bool isNull() const { return type() == Type::Null; }
    bool isNumber() const { return type() == Type::Int || type() == Type::Float; }

    /** Everything is true except tupu and sikweli (so 0 and "" are true). */
    bool isTruthy() const;

    bool asBool() const { return std::get<bool>(data_); }
    int64_t asInt() const { return std::get<int64_t>(data_); }
    double asFloat() const { return std::get<double>(data_); }
    double toDouble() const;
    const std::string& asString() const {
      return *std::get<std::shared_ptr<const std::string>>(data_);
    }
    Array& asArray() const { return *std::get<std::shared_ptr<Array>>(data_); }
    Dict& asDict() const { return *std::get<std::shared_ptr<Dict>>(data_); }
    Function& asFunction() const { return *std::get<std::shared_ptr<Function>>(data_); }
    Builtin& asBuiltin() const { return *std::get<std::shared_ptr<Builtin>>(data_); }
    Package& asPackage() const { return *std::get<std::shared_ptr<Package>>(data_); }

    /** Form used by andika(): strings are raw, containers show their strings quoted. */
    std::string display() const;
    std::string inspect() const;
    /** Nuru's name for the type, as returned by aina(): NAMBA, DESIMALI, NENO, ... */
    const char* typeName() const;

    /** Two values are the same object (arrays/dicts/functions compare by identity). */
    bool sameObject(const Value& other) const;

  private:
    using Data
        = std::variant<std::monostate, bool, int64_t, double, std::shared_ptr<const std::string>,
                       std::shared_ptr<Array>, std::shared_ptr<Dict>, std::shared_ptr<Function>,
                       std::shared_ptr<Builtin>, std::shared_ptr<Package>>;
    explicit Value(Data d) : data_(std::move(d)) {}
    void write(std::string& out, bool quoteStrings, std::vector<const void*>& path) const;

    Data data_;
  };

  struct Array {
    std::vector<Value> items;
  };

  /** Insertion-ordered dictionary. Keys are strings, integers, floats or booleans. */
  struct Dict {
    std::vector<std::pair<Value, Value>> entries;

    static bool hashable(const Value& key);
    Value* find(const Value& key);
    void set(const Value& key, Value value);

  private:
    std::unordered_map<std::string, size_t> index_;
  };

  struct Function {
    const FunctionLiteral* node;  // owned by the Interpreter's retained programs
    std::shared_ptr<Environment> closure;
  };

  struct Builtin {
    std::string name;
    BuiltinFn fn;
    bool acceptsNamed = false;  // keyword arguments are an error unless this is set
  };

  /**
   * A pakeji, native module, or object with members (file, time): a named scope.
   * `@` inside a pakeji refers to its Package value.
   */
  struct Package {
    std::string name;
    std::string typeName;  // what aina() reports: PAKEJI, FAILI, MUDA
    std::string text;      // display form; empty means "<typeName name>"
    std::shared_ptr<Environment> env;
  };

  /** Structural equality: 1 == 1.0, arrays and dicts compare by contents. */
  bool valuesEqual(const Value& a, const Value& b);

  /** Shortest decimal text that reads back as the same double ("0.6", "3", "1e+21"). */
  std::string formatFloat(double d);

  /** Splits UTF-8 text into code points (each as its own string). */
  std::vector<std::string> utf8Chars(const std::string& s);

}  // namespace mni
