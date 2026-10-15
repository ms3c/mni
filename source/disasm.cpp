#include <mni/interpreter.h>
#include <mni/parser.h>

#include <cstdio>
#include <string>

#include "bytecode.h"
#include "compiler.h"

namespace mni {

  namespace {
    std::string operands(const Proto& proto, const Instr& i) {
      auto child = [&](int index) {
        const Proto& c = *proto.children[static_cast<size_t>(index)];
        return c.name.empty() ? std::string("<anonymous>") : c.name;
      };
      switch (i.op) {
        case Op::Const:
          return "#" + std::to_string(i.b) + " " + proto.consts[static_cast<size_t>(i.b)].inspect();
        case Op::GetVar:
        case Op::DefineVar:
        case Op::DefineVarKeep:
        case Op::AssignVar:
        case Op::GetProp:
        case Op::SetProp:
          return symbolName(static_cast<Symbol>(i.b));
        case Op::JumpIfOwnDefined:
          return symbolName(static_cast<Symbol>(i.a)) + " -> " + std::to_string(i.b);
        case Op::GetSlot:
        case Op::DefineSlot:
        case Op::DefineSlotKeep:
        case Op::AssignSlot:
          return "$" + std::to_string(i.b) + " "
                 + symbolName(proto.locals[static_cast<size_t>(i.b)]);
        case Op::JumpIfSlotSet:
          return "$" + std::to_string(i.a) + " "
                 + symbolName(proto.locals[static_cast<size_t>(i.a)]) + " -> "
                 + std::to_string(i.b);
        case Op::BinOpK:
          return std::string(tokenTypeName(static_cast<TokenType>(i.a))) + " #"
                 + std::to_string(i.b) + " " + proto.consts[static_cast<size_t>(i.b)].inspect();
        case Op::JumpIfNotCmp:
          return std::string(tokenTypeName(static_cast<TokenType>(i.a))) + " -> "
                 + std::to_string(i.b);
        case Op::JumpIfNotCmpK:
          return std::string(tokenTypeName(static_cast<TokenType>(i.a & 0xff))) + " #"
                 + std::to_string(i.a >> 8) + " "
                 + proto.consts[static_cast<size_t>(i.a >> 8)].inspect() + " -> "
                 + std::to_string(i.b);
        case Op::BinOp:
        case Op::Prefix:
          return std::string(tokenTypeName(static_cast<TokenType>(i.a)));
        case Op::PopN:
        case Op::MakeArray:
        case Op::Call:
          return std::to_string(i.b);
        case Op::Jump:
        case Op::JumpIfFalse:
        case Op::JumpIfTrue:
        case Op::DefaultsBegin:
        case Op::JumpIfSwitchEq:
          return "-> " + std::to_string(i.b);
        case Op::IterNext:
          return "-> " + std::to_string(i.b);
        case Op::CallMethod: {
          const CallInfo& info = proto.calls[static_cast<size_t>(i.b)];
          std::string text = info.name + " " + std::to_string(info.positional);
          for (const std::string& name : info.named) text += " " + name + "=";
          return text;
        }
        case Op::MakeFunction:
        case Op::MakePackage:
          return child(i.b);
        case Op::Import:
          return proto.consts[static_cast<size_t>(i.b)].display();
        case Op::Throw:
          return (i.a ? "(call line) " : "") + proto.consts[static_cast<size_t>(i.b)].inspect();
        default:
          return {};
      }
    }

    const char* kindName(Proto::Kind kind) {
      switch (kind) {
        case Proto::Kind::Main:
          return "main";
        case Proto::Kind::Function:
          return "function";
        case Proto::Kind::Package:
          return "package";
        case Proto::Kind::Module:
          return "module";
      }
      return "?";
    }

    void list(const Proto& proto, std::string& out) {
      out += std::string("== ") + kindName(proto.kind)
             + (proto.name.empty() ? "" : " " + proto.name) + " (maxStack "
             + std::to_string(proto.maxStack) + ")\n";
      for (size_t pc = 0; pc < proto.code.size(); ++pc) {
        char head[64];
        std::snprintf(head, sizeof head, "%4zu  %-18s", pc, opName(proto.code[pc].op));
        out += head;
        const std::string args = operands(proto, proto.code[pc]);
        out += args;
        // pad so the line numbers line up
        out += std::string(args.size() < 24 ? 24 - args.size() : 1, ' ');
        out += "L" + std::to_string(proto.lines[pc]) + "\n";
      }
      for (const auto& c : proto.children) list(*c, out);
    }
  }  // namespace

  std::string disassemble(const Proto& proto) {
    std::string out;
    list(proto, out);
    return out;
  }

  std::string Interpreter::disassembleSource(const std::string& source) {
    Parser parser(source);
    Program program = parser.parseProgram();
    if (!parser.errors().empty()) throw SyntaxError(parser.errors());
    return disassemble(*compileProgram(program, Proto::Kind::Main));
  }

}  // namespace mni
