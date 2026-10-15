#include <mni/lexer.h>
#include <mni/repl.h>

namespace mni {

  namespace {
    // Number of ( [ { still open in `source`; > 0 means the statement is not finished.
    int openBrackets(const std::string& source) {
      int depth = 0;
      for (const Token& t : Lexer(source).tokenize()) {
        switch (t.type) {
          case TokenType::LParen:
          case TokenType::LBracket:
          case TokenType::LBrace:
            ++depth;
            break;
          case TokenType::RParen:
          case TokenType::RBracket:
          case TokenType::RBrace:
            --depth;
            break;
          default:
            break;
        }
      }
      return depth;
    }
  }  // namespace

  int runRepl(Interpreter& interp, std::istream& in, std::ostream& out, std::ostream& err) {
    std::string line;
    for (;;) {
      out << ">>> " << std::flush;
      if (!std::getline(in, line)) {
        out << "\n";
        return 0;
      }
      std::string source = line;
      while (openBrackets(source) > 0) {
        out << "... " << std::flush;
        if (!std::getline(in, line)) break;
        source += "\n" + line;
      }

      try {
        Value result = interp.run(source);
        if (!result.isNull()) out << result.inspect() << "\n";
      } catch (const SyntaxError& e) {
        for (const std::string& msg : e.errors) err << msg << "\n";
      } catch (const NuruError& e) {
        err << e.what() << "\n";
      } catch (const ExitRequest& e) {
        return e.code;
      }
    }
  }

}  // namespace mni
