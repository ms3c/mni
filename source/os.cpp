#include <cstdio>
#include <string>

#include "native.h"

#ifdef _WIN32
#  define MNI_POPEN _popen
#  define MNI_PCLOSE _pclose
#else
#  define MNI_POPEN popen
#  define MNI_PCLOSE pclose
#endif

namespace mni::native {

  Value createOs() {
    Value pkg = makePackage("os", "PAKEJI");

    // Runs a shell command and returns what it wrote to standard output.
    addFunction(pkg, "kimbiza",
                [](Interpreter& interp, std::vector<Value>& args, NamedArgs&, int line) {
                  expectArgs("kimbiza", args, 1, 1, line);
                  const std::string& command = stringArg("kimbiza", args[0], line);
                  interp.out()
                      << std::flush;  // keep our output ahead of anything the command prints itself

                  FILE* pipe = MNI_POPEN(command.c_str(), "r");
                  if (!pipe) throw NuruError(line, "Imeshindwa kuendesha amri: " + command);
                  std::string output;
                  char buffer[4096];
                  while (size_t n = std::fread(buffer, 1, sizeof buffer, pipe))
                    output.append(buffer, n);
                  MNI_PCLOSE(pipe);
                  return Value::string(std::move(output));
                });

    addFunction(
        pkg, "toka", [](Interpreter&, std::vector<Value>& args, NamedArgs&, int line) -> Value {
          expectArgs("toka", args, 0, 1, line);
          throw ExitRequest{args.empty() ? 0 : static_cast<int>(intArg("toka", args[0], line))};
        });

    return pkg;
  }

}  // namespace mni::native
