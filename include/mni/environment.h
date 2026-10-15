#pragma once

#include <mni/symbol.h>
#include <mni/value.h>

#include <memory>
#include <string>
#include <vector>

namespace mni {

  /**
   * A variable scope; scopes chain outward through `outer`.
   *
   * Variables are kept in a flat list searched linearly, which beats hashing for the handful of
   * names a function scope has. A scope that grows large (globals, modules) adds an index from
   * symbol to slot. The `std::string` overloads intern the name first and are for setup code; the
   * evaluator uses the `Symbol` ones.
   */
  class Environment : public std::enable_shared_from_this<Environment> {
  public:
    explicit Environment(std::shared_ptr<Environment> outer = nullptr, size_t expected = 0)
        : outer_(std::move(outer)) {
      slots_.reserve(expected);
    }

    Value* lookup(Symbol name) {
      for (Environment* e = this; e; e = e->outer_.get())
        if (Value* v = e->lookupOwn(name)) return v;
      return nullptr;
    }

    Value* lookupOwn(Symbol name) {
      if (!index_.empty()) {
        if (name >= index_.size() || index_[name] < 0) return nullptr;
        return &slots_[static_cast<size_t>(index_[name])].value;
      }
      for (Slot& slot : slots_)
        if (slot.symbol == name) return &slot.value;
      return nullptr;
    }

    /** Creates or overwrites a binding in this scope. */
    void define(Symbol name, Value value) {
      if (Value* v = lookupOwn(name)) {
        *v = std::move(value);
      } else {
        add(name, std::move(value));
      }
    }

    /**
     * Updates the nearest existing binding, or creates one in this scope. A boundary scope (a
     * pakeji or a loaded file) is never crossed: if the name is not found by then it is created
     * there, so a package cannot overwrite the importer's variables.
     */
    void assign(Symbol name, Value value) {
      for (Environment* e = this; e; e = e->outer_.get()) {
        if (Value* v = e->lookupOwn(name)) {
          *v = std::move(value);
          return;
        }
        if (e->boundary_) {
          e->add(name, std::move(value));
          return;
        }
      }
      define(name, std::move(value));
    }

    Value* lookup(const std::string& name) { return lookup(intern(name)); }
    Value* lookupOwn(const std::string& name) { return lookupOwn(intern(name)); }
    void define(const std::string& name, Value value) { define(intern(name), std::move(value)); }
    void assign(const std::string& name, Value value) { assign(intern(name), std::move(value)); }

    void setBoundary() { boundary_ = true; }

  private:
    struct Slot {
      Symbol symbol;
      Value value;
    };

    static constexpr size_t kLinearLimit = 8;

    void add(Symbol name, Value value);
    void setIndex(Symbol name, size_t slot);

    std::vector<Slot> slots_;
    std::vector<int32_t> index_;  // symbol -> slot, empty while the scope is small
    std::shared_ptr<Environment> outer_;
    bool boundary_ = false;
  };

}  // namespace mni
