#include <algorithm>
#include <string>
#include <vector>

#include "bytecode.h"

namespace mni {

  namespace {
    std::string where(const Proto& proto, size_t pc) {
      return (proto.name.empty() ? "<main>" : proto.name) + " at " + std::to_string(pc) + " (line "
             + std::to_string(pc < proto.lines.size() ? proto.lines[pc] : 0) + ")";
    }

    std::string verifyOne(const Proto& proto) {
      const size_t n = proto.code.size();
      if (n == 0) return "empty proto " + proto.name;
      if (proto.lines.size() != n) return "line table does not match the code in " + proto.name;

      std::vector<int> depth(n, -1);
      std::vector<size_t> work{0};
      depth[0] = 0;
      int deepest = 0;

      // Propagates the depth after an instruction to a successor; false if two paths disagree.
      auto reach = [&](size_t target, int after, std::string& problem, size_t from) {
        if (depth[target] == -1) {
          depth[target] = after;
          work.push_back(target);
        } else if (depth[target] != after) {
          problem = "stack depth " + std::to_string(depth[target]) + " vs " + std::to_string(after)
                    + " where paths meet at " + std::to_string(target) + ", reached from "
                    + where(proto, from);
          return false;
        }
        return true;
      };

      while (!work.empty()) {
        const size_t pc = work.back();
        work.pop_back();
        const Instr& instr = proto.code[pc];
        const int here = depth[pc];
        const StackEffect effect = stackEffect(proto, instr);
        if (here < effect.pops) {
          return std::string("stack underflow in ") + opName(instr.op) + " in " + where(proto, pc);
        }
        if (instr.op == Op::Return && here < 1)
          return "Return with an empty stack in " + where(proto, pc);

        const int after = here - effect.pops + effect.pushes;
        deepest = std::max({deepest, here, after});

        std::string problem;
        if (hasTarget(instr.op)) {
          if (instr.b < 0 || static_cast<size_t>(instr.b) >= n)
            return std::string("jump out of range in ") + opName(instr.op) + " in "
                   + where(proto, pc);
          if (!reach(static_cast<size_t>(instr.b), after, problem, pc)) return problem;
        }
        if (!endsFlow(instr.op)) {
          if (pc + 1 >= n) return "code runs off the end in " + where(proto, pc);
          if (!reach(pc + 1, after, problem, pc)) return problem;
        }
      }

      if (proto.maxStack < deepest)
        return "maxStack " + std::to_string(proto.maxStack) + " is below the real depth "
               + std::to_string(deepest) + " in " + proto.name;
      return {};
    }
  }  // namespace

  std::string verify(const Proto& proto) {
    std::string problem = verifyOne(proto);
    if (!problem.empty()) return problem;
    for (const auto& child : proto.children) {
      problem = verify(*child);
      if (!problem.empty()) return problem;
    }
    return {};
  }

}  // namespace mni
