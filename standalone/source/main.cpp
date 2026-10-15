#include <mni/interpreter.h>
#include <mni/lexer.h>
#include <mni/parser.h>
#include <mni/repl.h>
#include <mni/version.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

  constexpr const char* kHelp
      = "Namna ya kutumia Nuru:\n"
        "  nuru                       Kuanza programu ya Nuru (REPL)\n"
        "  nuru jinaLaFile.nr         Kuendesha faili la Nuru\n"
        "  nuru --tokens file.nr      Onyesha tokens za faili\n"
        "  nuru --ast file.nr         Onyesha AST ya faili\n"
        "  nuru --toleo               Kufahamu toleo la Nuru\n";

  bool hasNuruExtension(const std::string& path) {
    auto ends = [&](const std::string& suffix) {
      return path.size() >= suffix.size()
             && path.compare(path.size() - suffix.size(), suffix.size(), suffix) == 0;
    };
    return ends(".nr") || ends(".sw");
  }

  bool readFile(const std::string& path, std::string& out) {
    if (!hasNuruExtension(path)) {
      std::cerr << "'" << path << "' sii faili sahihi. Tumia faili la '.nr' au '.sw'\n";
      return false;
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
      std::cerr << "Error: Nuru imeshindwa kusoma faili: " << path << "\n";
      return false;
    }
    std::stringstream contents;
    contents << in.rdbuf();
    out = contents.str();
    return true;
  }

  int printTokens(const std::string& source) {
    for (const mni::Token& t : mni::Lexer(source).tokenize()) {
      std::cout << t.line << "\t" << mni::tokenTypeName(t.type) << "\t" << t.literal << "\n";
    }
    return 0;
  }

  // Returns false (after printing the errors) if the source does not parse.
  bool parse(const std::string& source, mni::Program& program) {
    mni::Parser parser(source);
    program = parser.parseProgram();
    for (const std::string& e : parser.errors()) std::cerr << e << "\n";
    return parser.errors().empty();
  }

}  // namespace

auto main(int argc, char** argv) -> int {
  if (argc < 2) {
    std::cout << "Nuru (mni) " << MNI_VERSION << "\nTumia toka() kuondoka\n";
    mni::Interpreter interp;
    return mni::runRepl(interp, std::cin, std::cout, std::cerr);
  }

  const std::string arg = argv[1];
  if (argc == 2) {
    if (arg == "-h" || arg == "--help" || arg == "--msaada" || arg == "-msaada"
        || arg == "msaada") {
      std::cout << kHelp;
      return 0;
    }
    if (arg == "-v" || arg == "--version" || arg == "--toleo" || arg == "-toleo" || arg == "v") {
      std::cout << "Nuru (mni) " << MNI_VERSION << "\n";
      return 0;
    }
  }

  const bool wantTokens = argc == 3 && arg == "--tokens";
  const bool wantAst = argc == 3 && arg == "--ast";
  if (argc > 3 || (argc == 3 && !wantTokens && !wantAst)) {
    std::cerr << kHelp;
    return 1;
  }

  std::string source;
  if (!readFile(argv[argc - 1], source)) return 1;

  if (wantTokens) return printTokens(source);

  if (wantAst) {
    mni::Program program;
    if (!parse(source, program)) return 1;
    std::cout << program.toString() << "\n";
    return 0;
  }

  mni::Interpreter interp;
  interp.setScriptPath(argv[argc - 1]);
  try {
    interp.run(source);
  } catch (const mni::SyntaxError& e) {
    std::cerr << e.what() << "\n";
    return 1;
  } catch (const mni::NuruError& e) {
    std::cerr << e.what() << "\n";
    return 1;
  } catch (const mni::ExitRequest& e) {
    return e.code;
  }
  return 0;
}
