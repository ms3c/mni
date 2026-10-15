#include <mni/environment.h>

#include <algorithm>

namespace mni {

  void Environment::add(Symbol name, Value value) {
    slots_.push_back(Slot{name, std::move(value)});
    if (!index_.empty()) {
      setIndex(name, slots_.size() - 1);
    } else if (slots_.size() > kLinearLimit) {
      for (size_t i = 0; i < slots_.size(); ++i) setIndex(slots_[i].symbol, i);
    }
  }

  void Environment::setIndex(Symbol name, size_t slot) {
    if (name >= index_.size()) index_.resize(std::max<size_t>(name + 1, index_.size() * 2), -1);
    index_[name] = static_cast<int32_t>(slot);
  }

}  // namespace mni
