#pragma once

// Internal: the bytecode the compiler produces and the VM runs. Not installed.
//
// Programs are compiled to a stack machine whose variable operations still go through the
// symbol-keyed Environment, so scoping behaves exactly as in the tree-walking evaluator.

#include <mni/ast.h>
#include <mni/symbol.h>
#include <mni/value.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace mni {

  // Stack effects are written [pops -> pushes]. "peek" means the operand stays on the stack.
  enum class Op : uint8_t {
    // constants and plain stack manipulation
    Const,  // b = constant index                              [0 -> 1]
    Null,   //                                                 [0 -> 1]
    True,   //                                                 [0 -> 1]
    False,  //                                                 [0 -> 1]
    Pop,    //                                                 [1 -> 0]
    PopN,   // b = count                                       [b -> 0]
    Dup,    //                                                 [1 -> 2]
    Dup2,   // duplicates the top two values                   [2 -> 4]

    // variables (b = symbol)
    GetVar,         // error if undefined                       [0 -> 1]
    DefineVar,      // define in the current scope              [1 -> 0]
    DefineVarKeep,  // define, value stays                      [1 -> 1]
    AssignVar,      // Environment::assign, value stays         [1 -> 1]
    LoadAt,         // the enclosing pakeji (`@`)               [0 -> 1]

    // Variables of a "slotted" function (see Proto::slotted): its own variables live in fixed
    // slots instead of an Environment. b = slot. A slot that was never set falls back to the
    // enclosing scopes exactly as the Environment would.
    GetSlot,         // GetVar                                   [0 -> 1]
    DefineSlot,      // DefineVar                                [1 -> 0]
    DefineSlotKeep,  // DefineVarKeep                            [1 -> 1]
    AssignSlot,      // AssignVar                                [1 -> 1]
    JumpIfSlotSet,   // a = slot, b = target (JumpIfOwnDefined)  [0 -> 0]

    // operators
    BinOp,   // a = TokenType; Interpreter::binaryOp            [2 -> 1]
    BinOpK,  // BinOp whose right operand is constant b          [1 -> 1]
    // A comparison fused with the JumpIfFalse after it (a = TokenType, b = target)
    JumpIfNotCmp,  // pops both operands                        [2 -> 0]
    // The same with a constant right operand: a = TokenType | (constant index << 8)
    JumpIfNotCmpK,  //                                           [1 -> 0]
    Prefix,         // a = TokenType (! - +)                           [1 -> 1]
    Incr,           // number + 1, else an error                       [1 -> 1]
    Decr,           // number - 1, else an error                       [1 -> 1]
    Truthy,         // replaces the top value with its truthiness      [1 -> 1]

    // indexing and properties
    GetIndex,  //                                               [2 -> 1]
    SetIndex,  // object index value -> value                   [3 -> 1]
    GetProp,   // b = symbol                                    [1 -> 1]
    SetProp,   // b = symbol; object value -> value             [2 -> 1]

    // construction
    MakeArray,     // b = element count                          [b -> 1]
    MakeDict,      // an empty dictionary                        [0 -> 1]
    DictSet,       // dict key value -> dict (line of the key)   [3 -> 1]
    MakeFunction,  // b = index into Proto::children             [0 -> 1]

    // jumps (b = absolute target)
    Jump,              //                                        [0 -> 0]
    JumpIfFalse,       //                                        [1 -> 0]
    JumpIfTrue,        //                                        [1 -> 0]
    JumpIfOwnDefined,  // a = symbol: jump if bound in this scope [0 -> 0]
    DefaultsBegin,     // jump to b if every parameter was given   [0 -> 0]

    // calls
    Call,        // b = argument count; callee below the args     [b+1 -> 1]
    CallMethod,  // b = index into Proto::calls                   [1+args -> 1]
    Return,      // pops the result and leaves the frame          [1 -> 0]

    // for-in: the three hidden slots are snapshot, cursor, stride
    IterInit,  // iterable -> snapshot cursor stride              [1 -> 3]
    IterNext,  // a = Proto::forIns index, b = exit target        [0 -> 0]
    IterPop,   //                                                 [3 -> 0]

    // switch
    JumpIfSwitchEq,  // b = target; pops the candidate, peeks the subject  [1 -> 0]

    // packages and imports
    MakePackage,  // b = index into Proto::children (the body)   [0 -> 1]
    Import,       // b = constant index (module name)            [0 -> 0]

    // a runtime error. a = 1 means "report it at the line of the call that entered this function"
    Throw,  // b = constant index (message)                      [0 -> 0]
  };

  // a and b are general purpose operands, see Op.
  struct Instr {
    Op op;
    int32_t a = 0;
    int32_t b = 0;
  };

  struct ParamInfo {
    Symbol symbol;
    std::string name;
    bool hasDefault;
  };

  struct ForInInfo {
    Symbol key;  // only meaningful if hasKey
    Symbol value;
    bool hasKey;
    int keySlot = -1;  // slotted functions only
    int valueSlot = -1;
  };

  // A method call `object.name(positional, name = value ...)`.
  struct CallInfo {
    Symbol method;
    std::string name;
    int positional;
    std::vector<std::string> named;  // keyword names, in source order
  };

  struct Proto {
    enum class Kind { Main, Function, Package, Module };

    Kind kind = Kind::Main;
    const FunctionLiteral* node = nullptr;  // functions only
    std::string name;                       // function or package name, for messages

    std::vector<Instr> code;
    std::vector<int> lines;  // one source line per instruction
    std::vector<Value> consts;
    std::vector<ParamInfo> params;
    std::vector<ForInInfo> forIns;
    std::vector<CallInfo> calls;
    std::vector<std::unique_ptr<Proto>> children;  // functions and package bodies

    // A function that creates no closure, package or import cannot be seen by anything else, so
    // its own variables are kept in `locals` slots on the VM's local stack. Set by slotify().
    bool slotted = false;
    std::vector<Symbol> locals;
    std::vector<int> paramSlots;  // slot of each parameter (two parameters may share a name)

    int maxStack = 0;  // deepest operand stack this proto needs (set by the compiler)
  };

  struct StackEffect {
    int pops;
    int pushes;
  };

  const char* opName(Op op);

  /** Turns the variable operations of a closure-free function into slot operations. */
  void slotify(Proto& proto);

  /** How many values `instr` pops and pushes, given the proto it belongs to. */
  StackEffect stackEffect(const Proto& proto, const Instr& instr);

  /** True for instructions after which execution never continues to the next one. */
  bool endsFlow(Op op);

  /** True if `op` has a jump target in operand b. */
  bool hasTarget(Op op);

  /** Human-readable listing of a proto and, recursively, its children. */
  std::string disassemble(const Proto& proto);

  /**
   * Checks the proto and its children are well formed: jump targets in range, the operand stack
   * has the same depth wherever paths meet and never goes negative. Returns an empty string if all
   * is well, otherwise a description of the first problem.
   */
  std::string verify(const Proto& proto);

}  // namespace mni
