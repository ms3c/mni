#include <fstream>
#include <sstream>

#include "native.h"

namespace mni {

  Value createNativeModule(const std::string& name) {
    if (name == "hisabati") return native::createHisabati();
    if (name == "jsoni") return native::createJsoni();
    if (name == "muda") return native::createMuda();
    if (name == "os") return native::createOs();
    if (name == "mtandao") return native::createMtandao();
    return Value();
  }

  bool isUnavailableModule(const std::string& name) {
#ifdef MNI_HAVE_HTTP
    (void)name;
    return false;
#else
    return name == "mtandao";
#endif
  }

  Value openFile(const std::string& path, int line) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw NuruError(line, "Imeshindwa kufungua faili: " + path);
    std::stringstream contents;
    contents << in.rdbuf();
    Value file = native::makePackage(path, "FAILI");
    native::addFunction(
        file, "soma",
        [text = contents.str()](Interpreter&, std::vector<Value>& args, NamedArgs&, int line2) {
          native::expectArgs("soma", args, 0, 0, line2);
          return Value::string(text);
        });
    return file;
  }

}  // namespace mni
