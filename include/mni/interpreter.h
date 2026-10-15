#pragma once

#include <mni/ast.h>
#include <mni/environment.h>
#include <mni/value.h>

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace mni {

  /** A runtime error: what() is "Mstari N: <message>". */
  class NuruError : public std::runtime_error {
  public:
    NuruError(int line, const std::string& message)
        : std::runtime_error("Mstari " + std::to_string(line) + ": " + message) {}
  };

  /** Parse failed: errors holds every parser message; what() joins them with newlines. */
  class SyntaxError : public std::runtime_error {
  public:
    explicit SyntaxError(std::vector<std::string> errors);
    std::vector<std::string> errors;
  };

  /** Thrown by toka()/exit() to leave the program. */
  struct ExitRequest {
    int code = 0;
  };

  /**
   * @brief Tree-walking evaluator
   *
   * Scoping: functions create a scope; blocks (kama, wakati, kwa, ...) do not. `x = v` updates the
   * nearest existing `x` or creates it in the current scope; `fanya x = v` always creates it in the
   * current scope. The AST of everything run() has seen is kept alive, because functions point
   * into it.
   */
  class Interpreter {
  public:
    explicit Interpreter(std::ostream& out = std::cout, std::istream& in = std::cin);
    ~Interpreter();
    Interpreter(const Interpreter&) = delete;
    Interpreter& operator=(const Interpreter&) = delete;

    /**
     * @brief Parses and executes `source` in the persistent global scope
     * @return the value of the last expression statement (tupu if there is none)
     * @throws SyntaxError, NuruError, ExitRequest
     */
    Value run(const std::string& source);

    Value callFunction(const Value& callee, std::vector<Value> args, int line,
                       NamedArgs named = {});

    /**
     * Tells the interpreter which file is being run, so `tumia name` finds `name.nr` next to it.
     * Without this, imports are looked up relative to the working directory.
     */
    void setScriptPath(const std::string& path);

    std::ostream& out() { return out_; }
    std::istream& in() { return in_; }

    /** Maximum nesting of Nuru function calls before "recursion too deep" (default 1000). */
    void setMaxCallDepth(int depth) { maxCallDepth_ = depth; }

  private:
    enum class Flow { None, Break, Continue, Return };

    Value exec(const Statement* stmt);
    Value execBlock(const BlockStatement* block);

    Value eval(const Expression* expr);
    Value evalIdentifier(const Identifier* id);
    Value evalPrefix(const PrefixExpression* e);
    Value evalInfix(const InfixExpression* e);
    Value evalAssign(const AssignExpression* e);
    Value evalPostfix(const PostfixExpression* e);
    Value evalIf(const IfExpression* e);
    Value evalWhile(const WhileExpression* e);
    Value evalForIn(const ForInExpression* e);
    Value evalSwitch(const SwitchExpression* e);
    Value evalFunction(const FunctionLiteral* e);
    Value evalCall(const CallExpression* e);
    Value evalMethodCall(const MethodCallExpression* e);
    Value evalArray(const ArrayLiteral* e);
    Value evalDict(const DictLiteral* e);
    Value evalProperty(const PropertyExpression* e);
    Value evalImport(const ImportExpression* e);
    Value evalPackage(const PackageExpression* e);
    Value evalAt(const AtExpression* e);

    // A place that can be read and written: `x`, `a[i]` or `o.p`.
    struct Ref {
      enum class Kind { Var, Index, Property } kind = Kind::Var;
      std::string name;  // variable or property name
      Value object;      // Index / Property
      Value index;       // Index
    };
    bool makeRef(const Expression* target, Ref& ref);  // false if a break/return interrupted it
    Value readRef(const Ref& ref, int line);
    void writeRef(const Ref& ref, const Value& value, int line);
    Value getProperty(const Value& object, const std::string& name, int line);
    void setProperty(const Value& object, const std::string& name, Value value, int line);

    Value binaryOp(TokenType op, const Value& left, const Value& right, int line);
    Value indexValue(const Value& object, const Value& index, int line);
    void setIndex(const Value& object, const Value& index, Value value, int line);
    Value callUser(const Value& fn, std::vector<Value>& args, NamedArgs& named, int line);
    Value execProgram(const Program& program);
    Value loadModule(const std::string& name, int line);
    Value importFile(const std::string& path, const std::string& packageName, int line);

    void installBuiltins();
    void defineBuiltin(const std::string& name, BuiltinFn fn, bool acceptsNamed = false);
    Value callMethod(const Value& self, const std::string& name, std::vector<Value>& args,
                     NamedArgs& named, int line);

    std::ostream& out_;
    std::istream& in_;
    std::shared_ptr<Environment> globals_;
    std::shared_ptr<Environment> env_;
    std::vector<std::unique_ptr<Program>> programs_;
    Flow flow_ = Flow::None;
    Value returnValue_;
    int callDepth_ = 0;
    int maxCallDepth_ = 1000;
    std::unordered_map<std::string, Value>
        modules_;                       // loaded packages, by native name or file path
    std::vector<std::string> loading_;  // files being imported (cycle detection)
    std::string currentDir_;            // where `tumia name` looks first
  };

}  // namespace mni
