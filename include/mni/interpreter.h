#pragma once

#include <mni/ast.h>
#include <mni/environment.h>
#include <mni/value.h>

#include <atomic>
#include <chrono>
#include <cstdint>
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

  /** Limits on how long a program may run. Each applies to every run() call separately. */
  struct ExecLimits {
    /** Ticks allowed: one per completed loop iteration and one per user function call. 0 = no
     * limit. */
    uint64_t maxTicks = 0;
    /** Wall-clock time allowed. 0 = no limit. */
    std::chrono::milliseconds timeout{0};
    /** If set, the program stops soon after this becomes true (it may be set from another thread).
     */
    const std::atomic<bool>* cancel = nullptr;
  };

  /** A program was stopped because it hit one of its ExecLimits. */
  class LimitError : public NuruError {
  public:
    enum class Kind { Ticks, Timeout, Cancelled };
    LimitError(Kind kind, int line, const std::string& message)
        : NuruError(line, message), kind_(kind) {}
    Kind kind() const { return kind_; }

  private:
    Kind kind_;
  };

  /** Thrown by toka()/exit() to leave the program. */
  struct ExitRequest {
    int code = 0;
  };

  class Vm;

  /**
   * @brief The Nuru interpreter, with two backends
   *
   * The bytecode VM is the default. The tree-walking evaluator (Backend::Tree, or MNI_BACKEND=tree)
   * gives the same results and is kept as a reference; the differences are in
   * docs/vm-divergences.md. The VM keeps Nuru calls on the heap, so deep recursion does not use
   * the C++ stack.
   *
   * Scoping: functions create a scope; blocks (kama, wakati, kwa, ...) do not. `x = v` updates the
   * nearest existing `x` or creates it in the current scope; `fanya x = v` always creates it in the
   * current scope. The AST of everything run() has seen is kept alive, because functions point
   * into it.
   */
  class Interpreter {
  public:
    /** The engine that runs programs. */
    enum class Backend {
      Default,  // the MNI_BACKEND environment variable ("vm" or "tree"); the VM if unset
      Tree,     // the tree-walking evaluator
      Vm        // the bytecode VM
    };

    struct Options {
      Backend backend = Backend::Default;
    };

    explicit Interpreter(std::ostream& out = std::cout, std::istream& in = std::cin);
    Interpreter(std::ostream& out, std::istream& in, Options options);
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

    /** The engine in use: Tree or Vm, never Default. */
    Backend backend() const { return backend_; }

    /**
     * Compiles `source` to bytecode and returns a readable listing (for debugging and tests).
     * @throws SyntaxError
     */
    static std::string disassembleSource(const std::string& source);

    /** False in builds without the bytecode VM, where requesting it falls back to the tree. */
    static bool vmAvailable();

    /** Maximum nesting of Nuru function calls before "recursion too deep" (default 1000). */
    void setMaxCallDepth(int depth) { maxCallDepth_ = depth; }

    /**
     * Limits for the runs that follow. A program that hits one is stopped with a LimitError, and
     * the interpreter can be used again. Natives such as a shell command or an HTTP request are
     * not interrupted while they run; those that can wait (muda.lala) check for you.
     */
    void setLimits(const ExecLimits& limits) { limits_ = limits; }
    const ExecLimits& limits() const { return limits_; }

    /** For long-running native code: throws LimitError if the program was cancelled or timed out.
     */
    void checkInterrupt(int line = 0);

  private:
    friend class Vm;
    enum class Flow { None, Break, Continue, Return };

    // Counts a tick (see ExecLimits) and, every so often, checks the limits.
    void tick(int line) {
      if (--fuel_ < 0) refuel(line);
    }
    void refuel(int line);
    void startRun();

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
      Symbol symbol = 0;  // variable or property name
      Value object;       // Index / Property
      Value index;        // Index
    };
    bool makeRef(const Expression* target, Ref& ref);  // false if a break/return interrupted it
    Value readRef(const Ref& ref, int line);
    void writeRef(const Ref& ref, const Value& value, int line);
    Value getProperty(const Value& object, Symbol name, int line);
    void setProperty(const Value& object, Symbol name, Value value, int line);

    Value binaryOp(TokenType op, const Value& left, const Value& right, int line);
    Value indexValue(const Value& object, const Value& index, int line);
    void setIndex(const Value& object, const Value& index, Value value, int line);
    Value callUser(const Value& fn, std::vector<Value>& args, NamedArgs& named, int line);
    // Runs a user function whose parameters are (partly) bound in `local`; `bound` counts them.
    Value runUser(const Value& fn, const std::shared_ptr<Environment>& local, size_t bound,
                  int line);
    Value execProgram(const Program& program);
    Value loadModule(const std::string& name, int line);
    Value importFile(const std::string& path, const std::string& packageName, int line);

    void installBuiltins();
    void defineBuiltin(const std::string& name, BuiltinFn fn, bool acceptsNamed = false);
    Value callMethod(const Value& self, const std::string& name, std::vector<Value>& args,
                     NamedArgs& named, int line);

    Backend backend_ = Backend::Tree;
    std::unique_ptr<Vm> vm_;  // only when backend_ == Vm
    std::ostream& out_;
    std::istream& in_;
    std::shared_ptr<Environment> globals_;
    Environment* env_;  // the current scope; whoever switches it keeps the owning shared_ptr alive
    std::vector<std::unique_ptr<Program>> programs_;
    Flow flow_ = Flow::None;
    Value returnValue_;
    int callDepth_ = 0;
    int maxCallDepth_ = 1000;
    ExecLimits limits_;
    int64_t fuel_ = 0;                 // ticks left before the next limit check
    uint64_t ticksLeft_ = UINT64_MAX;  // ticks not yet handed out as fuel
    std::chrono::steady_clock::time_point deadline_;
    bool hasDeadline_ = false;
    std::unordered_map<std::string, Value>
        modules_;                       // loaded packages, by native name or file path
    std::vector<std::string> loading_;  // files being imported (cycle detection)
    std::string currentDir_;            // where `tumia name` looks first
  };

}  // namespace mni
