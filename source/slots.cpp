#include <unordered_map>

#include "bytecode.h"

namespace mni {

  void slotify(Proto& p) {
    // Anything that can capture or expose the function's scope keeps the Environment.
    for (const Instr& i : p.code) {
      switch (i.op) {
        case Op::MakeFunction:
        case Op::MakePackage:
        case Op::Import:
        case Op::LoadAt:
          return;
        default:
          break;
      }
    }

    std::unordered_map<Symbol, int> slots;
    p.locals.clear();
    auto slotOf = [&](Symbol symbol) {
      auto [it, added] = slots.try_emplace(symbol, static_cast<int>(p.locals.size()));
      if (added) p.locals.push_back(symbol);
      return it->second;
    };

    // names the function can bind in its own scope get a slot
    p.paramSlots.clear();
    for (const ParamInfo& param : p.params) p.paramSlots.push_back(slotOf(param.symbol));
    for (const Instr& i : p.code) {
      switch (i.op) {
        case Op::DefineVar:
        case Op::DefineVarKeep:
        case Op::AssignVar:
          slotOf(static_cast<Symbol>(i.b));
          break;
        case Op::JumpIfOwnDefined:
          slotOf(static_cast<Symbol>(i.a));
          break;
        default:
          break;
      }
    }
    for (ForInInfo& info : p.forIns) {
      if (info.hasKey) info.keySlot = slotOf(info.key);
      info.valueSlot = slotOf(info.value);
    }

    for (Instr& i : p.code) {
      switch (i.op) {
        case Op::GetVar: {
          auto it = slots.find(static_cast<Symbol>(i.b));
          if (it != slots.end()) {
            i.op = Op::GetSlot;
            i.b = it->second;
          }
          break;
        }
        case Op::DefineVar:
          i.op = Op::DefineSlot;
          i.b = slots.at(static_cast<Symbol>(i.b));
          break;
        case Op::DefineVarKeep:
          i.op = Op::DefineSlotKeep;
          i.b = slots.at(static_cast<Symbol>(i.b));
          break;
        case Op::AssignVar:
          i.op = Op::AssignSlot;
          i.b = slots.at(static_cast<Symbol>(i.b));
          break;
        case Op::JumpIfOwnDefined:
          i.op = Op::JumpIfSlotSet;
          i.a = slots.at(static_cast<Symbol>(i.a));
          break;
        default:
          break;
      }
    }
    p.slotted = true;
  }

}  // namespace mni
