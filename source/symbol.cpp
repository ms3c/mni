#include <mni/symbol.h>

#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>

namespace mni {

  namespace {
    struct SymbolTable {
      std::mutex mutex;
      std::unordered_map<std::string, Symbol> ids;
      std::deque<std::string> names;  // a deque keeps references to names valid as it grows
    };

    SymbolTable& table() {
      static SymbolTable instance;
      return instance;
    }
  }  // namespace

  Symbol intern(std::string_view name) {
    SymbolTable& t = table();
    std::lock_guard<std::mutex> lock(t.mutex);
    std::string key(name);
    auto it = t.ids.find(key);
    if (it != t.ids.end()) return it->second;
    const auto id = static_cast<Symbol>(t.names.size());
    t.names.push_back(key);
    t.ids.emplace(std::move(key), id);
    return id;
  }

  const std::string& symbolName(Symbol symbol) {
    SymbolTable& t = table();
    std::lock_guard<std::mutex> lock(t.mutex);
    return t.names.at(symbol);
  }

  std::size_t symbolCount() {
    SymbolTable& t = table();
    std::lock_guard<std::mutex> lock(t.mutex);
    return t.names.size();
  }

}  // namespace mni
