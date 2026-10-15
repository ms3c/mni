#pragma once

#include <mni/value.h>

#include <memory>
#include <string>
#include <unordered_map>

namespace mni {

  /** A variable scope; scopes chain outward through `outer`. */
  class Environment {
  public:
    explicit Environment(std::shared_ptr<Environment> outer = nullptr) : outer_(std::move(outer)) {}

    Value* lookup(const std::string& name) {
      for (Environment* e = this; e; e = e->outer_.get()) {
        auto it = e->vars_.find(name);
        if (it != e->vars_.end()) return &it->second;
      }
      return nullptr;
    }

    void define(const std::string& name, Value value) { vars_[name] = std::move(value); }

    Value* lookupOwn(const std::string& name) {
      auto it = vars_.find(name);
      return it == vars_.end() ? nullptr : &it->second;
    }

    /**
     * Updates the nearest existing binding, or creates one in this scope. A boundary scope (a
     * pakeji or a loaded file) is never crossed: if the name is not found by then it is created
     * there, so a package cannot overwrite the importer's variables.
     */
    void assign(const std::string& name, Value value) {
      for (Environment* e = this; e; e = e->outer_.get()) {
        if (Value* v = e->lookupOwn(name)) {
          *v = std::move(value);
          return;
        }
        if (e->boundary_) {
          e->vars_[name] = std::move(value);
          return;
        }
      }
      define(name, std::move(value));
    }

    void setBoundary() { boundary_ = true; }

  private:
    std::unordered_map<std::string, Value> vars_;
    std::shared_ptr<Environment> outer_;
    bool boundary_ = false;
  };

}  // namespace mni
