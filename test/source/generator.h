#pragma once

// A seeded generator of random Nuru programs, for compiler and differential tests. The programs
// are syntactically valid and use every construct, with a bias towards the awkward ones: vunja,
// endelea and rudisha inside expression operands, defaults, switch with kawaida first, compound
// assignment on indexes and properties.
//
// The same seed always gives the same program, so a failure can be reproduced from its seed.

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace mni::testing {

  class ProgramGenerator {
  public:
    /**
     * With `executable` set, every loop is bounded and the program always terminates, so it can
     * be run. Otherwise the programs only need to compile. With `numeric` set the values are
     * mostly numbers, so the programs rarely stop at a type error and run much further.
     */
    explicit ProgramGenerator(uint64_t seed, bool executable = false, bool numeric = false)
        : executable_(executable),
          numeric_(numeric),
          state_(seed * 2862933555777941757ULL + 3037000493ULL) {
      for (int i = 0; i < 4; ++i) next();
    }

    /** A whole program of roughly `statements` top-level statements. */
    std::string program(int statements) {
      // every name the programs use is defined, so they get past their first statement
      std::string out
          = "a = 1; b = 2; c = 3; i = 0; s = \"s\"; t = 5; xs = [1, 2, 3]; d = {\"k\": 1}; "
            "f = unda(a, b = 2) { rudisha a }; "
            "pakeji P { n = 0; m = unda(a, b = 1) { rudisha a } }; ";
      for (int i = 0; i < statements; ++i) out += statement(3) + "; ";
      return out;
    }

  private:
    uint64_t next() {
      state_ ^= state_ << 13;
      state_ ^= state_ >> 7;
      state_ ^= state_ << 17;
      return state_;
    }

    int pick(int n) { return static_cast<int>(next() % static_cast<uint64_t>(n)); }
    bool chance(int percent) { return pick(100) < percent; }

    std::string name() {
      static const char* const names[] = {"a", "b", "c", "i", "s", "t"};
      return names[pick(6)];
    }

    std::string blockBody(int depth, int statements) {
      std::string out;
      for (int i = 0; i < statements; ++i) out += statement(depth) + "; ";
      return out;
    }

    std::string block(int depth, int statements) {
      return "{ " + blockBody(depth, statements) + "}";
    }

    std::string literal() {
      if (numeric_ && chance(90)) return chance(30) ? "2.5" : std::to_string(pick(5));
      switch (pick(8)) {
        case 0:
          return "0";
        case 1:
          return "1";
        case 2:
          return std::to_string(pick(100));
        case 3:
          return "2.5";
        case 4:
          return "\"s\"";
        case 5:
          return "kweli";
        case 6:
          return "sikweli";
        default:
          return "tupu";
      }
    }

    std::string expression(int depth) {
      if (depth <= 0) return chance(50) ? literal() : name();
      switch (pick(24)) {
        case 0:
        case 1:
          return literal();
        case 2:
        case 3:
          return name();
        case 4:
        case 5:
        case 6: {
          static const char* const ops[] = {"+", "-", "*", "<", "==", "&&", "||", "ktk", "%", ">="};
          // "ktk" needs a container on the right, so numeric programs leave it out
          return "(" + expression(depth - 1) + " " + ops[pick(numeric_ ? 9 : 10)] + " "
                 + expression(depth - 1) + ")";
        }
        case 7:
          return std::string(chance(50) ? "!" : "-") + expression(depth - 1);
        case 8:
          return "[" + expression(depth - 1) + ", " + expression(depth - 1) + "]";
        case 9:
          return "{\"k\": " + expression(depth - 1) + ", " + expression(depth - 1) + ": 1}";
        case 10:
          return "xs[" + expression(depth - 1) + "]";
        case 11:
          return "f(" + expression(depth - 1) + ", " + expression(depth - 1) + ")";
        case 12:
          return "xs.sukuma(" + expression(depth - 1) + ")";
        case 13:
          return "P.m(" + expression(depth - 1) + ", b = " + expression(depth - 1) + ")";
        case 14:
          return "(" + name() + " = " + expression(depth - 1) + ")";
        case 15: {
          static const char* const ops[] = {"+=", "-=", "*=", "/=", "%="};
          return "(" + name() + " " + ops[pick(5)] + " " + expression(depth - 1) + ")";
        }
        case 16:
          return "(xs[" + expression(depth - 1) + "] " + (chance(50) ? "+=" : "=") + " "
                 + expression(depth - 1) + ")";
        case 17:
          return "(P.n " + std::string(chance(50) ? "+=" : "=") + " " + expression(depth - 1) + ")";
        case 18:
          return name() + (chance(50) ? "++" : "--");
        case 19:
          return "xs[" + expression(depth - 1) + "]" + (chance(50) ? "++" : "--");
        case 20:
          return ifExpression(depth);
        case 21:
          return switchExpression(depth);
        case 22:
          return function(depth);
        default:
          return "P.n" + std::string(chance(50) ? "++" : "");
      }
    }

    std::string ifExpression(int depth) {
      std::string out = "kama (" + expression(depth - 1) + ") " + block(depth - 1, 1 + pick(2));
      if (chance(60)) out += " sivyo " + block(depth - 1, 1 + pick(2));
      return out;
    }

    std::string switchExpression(int depth) {
      std::string out = "badili (" + expression(depth - 1) + ") { ";
      const bool defaultFirst = chance(40);
      const std::string deflt = "kawaida " + block(depth - 1, 1);
      if (defaultFirst) out += deflt + " ";
      for (int i = 0, n = 1 + pick(3); i < n; ++i) {
        out += "ikiwa " + expression(depth - 1);
        if (chance(30)) out += ", " + expression(depth - 1);
        out += " " + block(depth - 1, 1 + pick(2)) + " ";
      }
      if (!defaultFirst && chance(50)) out += deflt + " ";
      return out + "}";
    }

    std::string function(int depth) {
      const int outerLoops = std::exchange(loopDepth_, 0);  // vunja cannot cross a function
      ++functionDepth_;
      std::string out = chance(30) ? "unda g(" : "unda(";
      const int params = pick(3);
      for (int i = 0; i < params; ++i) {
        if (i) out += ", ";
        out += std::string("p") + std::to_string(i);
        if (i >= 1 || chance(30)) {
          // The tree-walker does not check for a pending vunja/endelea/rudisha while it fills in
          // a default, the VM does: a known, documented difference, so keep flow out of defaults.
          ++noFlow_;
          out += " = " + expression(depth - 1);
          --noFlow_;
        }
      }
      const int outer = std::exchange(noFlow_, 0);  // a function body is a fresh start
      out += ") " + block(depth - 1, 1 + pick(3));
      noFlow_ = outer;
      --functionDepth_;
      loopDepth_ = outerLoops;
      return out;
    }

    // wakati: when the program is to be run the condition is a private counter that is bumped
    // first thing in the body, so even endelea cannot make the loop endless.
    std::string loop(int depth) {
      if (!executable_) {
        ++loopDepth_;
        const std::string body = block(depth - 1, 1 + pick(3));
        --loopDepth_;
        return "wakati (" + expression(depth - 1) + ") " + body;
      }
      const std::string counter = "w" + std::to_string(counters_++);
      ++loopDepth_;
      const std::string body = blockBody(depth - 1, 1 + pick(3));
      --loopDepth_;
      return counter + " = 0; wakati (" + counter + " < 3) { " + counter + "++; " + body + "}";
    }

    std::string forIn(int depth) {
      std::string iterable = expression(depth - 1);
      if (executable_ && chance(85)) {
        static const char* const sources[] = {"xs", "d", "\"ab\"", "[1, 2]", "[]"};
        iterable = sources[pick(5)];
      }
      ++loopDepth_;
      const std::string body = block(depth - 1, 1 + pick(3));
      --loopDepth_;
      return "kwa " + std::string(chance(50) ? "k, v" : "v") + " ktk " + iterable + " " + body;
    }

    std::string statement(int depth) {
      int kind = pick(14);
      if (noFlow_ > 0 && (kind == 6 || kind == 7 || kind == 8))
        kind = 1;  // no vunja/endelea/rudisha
      // Keep flow statements where they belong, apart from the odd misuse to exercise the errors.
      if ((kind == 6 || kind == 7) && loopDepth_ == 0 && !chance(5)) kind = 1;
      if (kind == 8 && functionDepth_ == 0 && !chance(5)) kind = 1;
      switch (kind) {
        case 0:
          return "fanya " + name() + " = " + expression(depth);
        case 1:
        case 2:
        case 3:
          return expression(depth);
        case 4:
          return loop(depth);
        case 5:
          return forIn(depth);
        case 6:
          return "vunja";
        case 7:
          return "endelea";
        case 8:
          return chance(50) ? "rudisha" : "rudisha " + expression(depth);
        case 9:
          return ifExpression(depth);
        case 10:
          return switchExpression(depth);
        case 11:
          return "tumia hisabati";
        case 12:
          return name() + " = " + expression(depth);
        default:
          return "andika(" + expression(depth) + ")";
      }
    }

    bool executable_;
    bool numeric_;
    int counters_ = 0;
    int noFlow_ = 0;  // > 0 while generating a parameter default
    int loopDepth_ = 0;
    int functionDepth_ = 0;
    uint64_t state_;
  };

}  // namespace mni::testing
