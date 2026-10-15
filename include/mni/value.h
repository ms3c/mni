#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
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
  struct Proto;

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
    // Keep Package last: tag values are used as array indexes in a few places.
    enum class Type : uint8_t {
      Null,
      Bool,
      Int,
      Float,
      String,
      Array,
      Dict,
      Function,
      Builtin,
      Package
    };

    Value() = default;

    static Value boolean(bool b) {
      Value v;
      v.type_ = Type::Bool;
      v.scalar_.b = b;
      return v;
    }
    static Value integer(int64_t i) {
      Value v;
      v.type_ = Type::Int;
      v.scalar_.i = i;
      return v;
    }
    static Value real(double d) {
      Value v;
      v.type_ = Type::Float;
      v.scalar_.d = d;
      return v;
    }
    static Value string(std::string s);
    /** Shares an existing string (used for literals, so evaluating one allocates nothing). */
    static Value string(std::shared_ptr<const std::string> s);
    static Value array(std::vector<Value> items = {});
    static Value dict();
    static Value function(std::shared_ptr<Function> f);
    static Value builtin(std::shared_ptr<Builtin> b);
    static Value package(std::shared_ptr<Package> p);
    static Value native(std::string name, BuiltinFn fn, bool acceptsNamed = false);

    Type type() const { return type_; }
    bool isNull() const { return type_ == Type::Null; }
    bool isNumber() const { return type_ == Type::Int || type_ == Type::Float; }

    /** Everything is true except tupu and sikweli (so 0 and "" are true). */
    bool isTruthy() const { return !(type_ == Type::Null || (type_ == Type::Bool && !scalar_.b)); }

    bool asBool() const { return scalar_.b; }
    int64_t asInt() const { return scalar_.i; }
    double asFloat() const { return scalar_.d; }
    double toDouble() const {
      return type_ == Type::Int ? static_cast<double>(scalar_.i) : scalar_.d;
    }
    const std::string& asString() const { return *static_cast<const std::string*>(ref_.get()); }
    Array& asArray() const { return *static_cast<Array*>(ref_.get()); }
    Dict& asDict() const { return *static_cast<Dict*>(ref_.get()); }
    Function& asFunction() const { return *static_cast<Function*>(ref_.get()); }
    Builtin& asBuiltin() const { return *static_cast<Builtin*>(ref_.get()); }
    Package& asPackage() const { return *static_cast<Package*>(ref_.get()); }

    /** Form used by andika(): strings are raw, containers show their strings quoted. */
    std::string display() const;
    std::string inspect() const;
    /** Nuru's name for the type, as returned by aina(): NAMBA, DESIMALI, NENO, ... */
    const char* typeName() const;

    /** Two values are the same object (arrays/dicts/functions compare by identity). */
    bool sameObject(const Value& other) const;

  private:
    void write(std::string& out, bool quoteStrings, std::vector<const void*>& path) const;

    // Scalars live inline; everything else is a shared, type-erased heap object. A copy of a
    // number or boolean only copies a null pointer, with no reference counting.
    union Scalar {
      int64_t i = 0;
      bool b;
      double d;
    };

    Type type_ = Type::Null;
    Scalar scalar_;
    std::shared_ptr<void> ref_;
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
    const Proto* proto = nullptr;  // compiled body; only set when the bytecode VM made the function
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
