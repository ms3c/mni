#include "bytecode.h"

namespace mni {

  const char* opName(Op op) {
    switch (op) {
      case Op::Const:
        return "Const";
      case Op::Null:
        return "Null";
      case Op::True:
        return "True";
      case Op::False:
        return "False";
      case Op::Pop:
        return "Pop";
      case Op::PopN:
        return "PopN";
      case Op::Dup:
        return "Dup";
      case Op::Dup2:
        return "Dup2";
      case Op::BinOpK:
        return "BinOpK";
      case Op::JumpIfNotCmp:
        return "JumpIfNotCmp";
      case Op::JumpIfNotCmpK:
        return "JumpIfNotCmpK";
      case Op::GetVar:
        return "GetVar";
      case Op::GetSlot:
        return "GetSlot";
      case Op::DefineSlot:
        return "DefineSlot";
      case Op::DefineSlotKeep:
        return "DefineSlotKeep";
      case Op::AssignSlot:
        return "AssignSlot";
      case Op::JumpIfSlotSet:
        return "JumpIfSlotSet";
      case Op::DefineVar:
        return "DefineVar";
      case Op::DefineVarKeep:
        return "DefineVarKeep";
      case Op::AssignVar:
        return "AssignVar";
      case Op::LoadAt:
        return "LoadAt";
      case Op::BinOp:
        return "BinOp";
      case Op::Prefix:
        return "Prefix";
      case Op::Incr:
        return "Incr";
      case Op::Decr:
        return "Decr";
      case Op::Truthy:
        return "Truthy";
      case Op::GetIndex:
        return "GetIndex";
      case Op::SetIndex:
        return "SetIndex";
      case Op::GetProp:
        return "GetProp";
      case Op::SetProp:
        return "SetProp";
      case Op::MakeArray:
        return "MakeArray";
      case Op::MakeDict:
        return "MakeDict";
      case Op::DictSet:
        return "DictSet";
      case Op::MakeFunction:
        return "MakeFunction";
      case Op::Jump:
        return "Jump";
      case Op::JumpIfFalse:
        return "JumpIfFalse";
      case Op::JumpIfTrue:
        return "JumpIfTrue";
      case Op::JumpIfOwnDefined:
        return "JumpIfOwnDefined";
      case Op::DefaultsBegin:
        return "DefaultsBegin";
      case Op::Call:
        return "Call";
      case Op::CallMethod:
        return "CallMethod";
      case Op::Return:
        return "Return";
      case Op::IterInit:
        return "IterInit";
      case Op::IterNext:
        return "IterNext";
      case Op::IterPop:
        return "IterPop";
      case Op::JumpIfSwitchEq:
        return "JumpIfSwitchEq";
      case Op::MakePackage:
        return "MakePackage";
      case Op::Import:
        return "Import";
      case Op::Throw:
        return "Throw";
    }
    return "?";
  }

  StackEffect stackEffect(const Proto& proto, const Instr& instr) {
    switch (instr.op) {
      case Op::Const:
      case Op::Null:
      case Op::True:
      case Op::False:
      case Op::LoadAt:
      case Op::GetVar:
      case Op::GetSlot:
      case Op::MakeDict:
      case Op::MakeFunction:
      case Op::MakePackage:
        return {0, 1};
      case Op::Pop:
      case Op::DefineVar:
      case Op::DefineSlot:
      case Op::JumpIfFalse:
      case Op::JumpIfTrue:
      case Op::JumpIfSwitchEq:
      case Op::Return:
        return {1, 0};
      case Op::PopN:
        return {instr.b, 0};
      case Op::Dup:
        return {1, 2};
      case Op::Dup2:
        return {2, 4};
      case Op::DefineVarKeep:
      case Op::DefineSlotKeep:
      case Op::AssignVar:
      case Op::AssignSlot:
      case Op::Prefix:
      case Op::Incr:
      case Op::Decr:
      case Op::Truthy:
      case Op::GetProp:
        return {1, 1};
      case Op::JumpIfNotCmp:
        return {2, 0};
      case Op::JumpIfNotCmpK:
        return {1, 0};
      case Op::BinOpK:
        return {1, 1};
      case Op::BinOp:
      case Op::GetIndex:
      case Op::SetProp:
        return {2, 1};
      case Op::SetIndex:
      case Op::DictSet:
        return {3, 1};
      case Op::MakeArray:
        return {instr.b, 1};
      case Op::Call:
        return {instr.b + 1, 1};
      case Op::CallMethod: {
        const CallInfo& info = proto.calls[static_cast<size_t>(instr.b)];
        return {1 + info.positional + static_cast<int>(info.named.size()), 1};
      }
      case Op::IterInit:
        return {1, 3};
      case Op::IterPop:
        return {3, 0};
      case Op::Jump:
      case Op::JumpIfOwnDefined:
      case Op::JumpIfSlotSet:
      case Op::DefaultsBegin:
      case Op::IterNext:
      case Op::Import:
      case Op::Throw:
        return {0, 0};
    }
    return {0, 0};
  }

  bool endsFlow(Op op) { return op == Op::Jump || op == Op::Return || op == Op::Throw; }

  bool hasTarget(Op op) {
    switch (op) {
      case Op::Jump:
      case Op::JumpIfFalse:
      case Op::JumpIfTrue:
      case Op::JumpIfOwnDefined:
      case Op::JumpIfSlotSet:
      case Op::JumpIfNotCmp:
      case Op::JumpIfNotCmpK:
      case Op::DefaultsBegin:
      case Op::IterNext:
      case Op::JumpIfSwitchEq:
        return true;
      default:
        return false;
    }
  }

}  // namespace mni
