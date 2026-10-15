#pragma once

// Internal: the bytecode VM backend. Not installed.
//
// One Vm belongs to one Interpreter and holds no global state. Nuru-to-Nuru calls push a Frame on
// the heap, so recursion depth does not use the C++ stack; only native code that calls back into
// Nuru (map, chuja) re-enters `execute` recursively.

#include <mni/interpreter.h>

#include <memory>
#include <vector>

#include "bytecode.h"

namespace mni {

  class Vm {
  public:
    explicit Vm(Interpreter& interpreter) : in_(interpreter) {}

    /** Compiles and runs `program` in the global scope; returns its last statement's value. */
    Value runProgram(const Program& program);

    /** Runs an imported file's program in `scope` (its own boundary scope). */
    void runModule(const Program& program, const std::shared_ptr<Environment>& scope);

    /** Calls a Nuru function from native code (Interpreter::callFunction for user functions). */
    Value callFunction(const Value& callee, std::vector<Value> args, int line, NamedArgs named);

    /** Forgets all frames and values after an error, so the Interpreter can be used again. */
    void reset();

  private:
    struct Frame {
      const Proto* proto = nullptr;
      size_t pc = 0;
      size_t base = 0;       // operand stack height when the frame started
      size_t localBase = 0;  // where this frame's slots start in locals_ (slotted functions)
      Environment* env = nullptr;
      std::shared_ptr<Environment> owner;  // keeps a call's own scope alive
      int callLine = 0;                    // line of the call that entered this frame
      size_t bound = 0;                    // how many parameters the caller supplied
      int depthBefore = 0;                 // Interpreter::callDepth_ before this frame
      bool native = false;                 // returning from this frame returns from execute()

      // package bodies: the package being built and where its name gets defined
      Value package;
      Environment* parentEnv = nullptr;
      Symbol packageName = 0;
    };

    /** Runs until the innermost `native` frame returns, and gives back its result. */
    Value execute();
    Value dispatch();  // the instruction loop; execute() adds error cleanup around it

    void truncateLocals(size_t size);
    void checkCallLimits(int line, bool native);
    void unwindTo(size_t frameCount, size_t stackHeight, int callDepth);

    /** Pushes a native frame for a top-level proto in `scope` and runs it. */
    Value runTopLevel(std::unique_ptr<Proto> proto, Environment* scope,
                      const std::shared_ptr<Environment>& owner);

    // calls
    void callFromStack(const Value& callee, size_t calleeIndex, size_t argc, int line);
    void callWithNamed(const Value& callee, std::vector<Value>& args, NamedArgs& named, int line,
                       bool native);
    void pushFunctionFrame(const Proto* proto, const std::shared_ptr<Environment>& scope,
                           size_t bound, int line, bool native);
    void callMethod(const Instr& instr, const Proto& proto, int line);

    Value pop() {
      Value v = std::move(stack_.back());
      stack_.pop_back();
      return v;
    }

    Interpreter& in_;
    std::vector<Value> stack_;
    std::vector<Frame> frames_;
    std::vector<Value> locals_;      // the slots of every running slotted function
    std::vector<uint8_t> localSet_;  // whether each slot has been set (an unset slot is not tupu)
    std::vector<std::unique_ptr<Proto>> protos_;  // top-level protos; they outlive every closure
  };

}  // namespace mni
