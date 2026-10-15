#pragma once

#include <mni/symbol.h>
#include <mni/value.h>

#include <cstddef>
#include <memory>
#include <new>
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
    Environment(const Environment&) = delete;
    Environment& operator=(const Environment&) = delete;

    /** Creates a scope for a function call. Freed scopes are recycled per thread. */
    static std::shared_ptr<Environment> make(std::shared_ptr<Environment> outer, size_t expected);

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

    /**
     * The outward half of assign(): updates the nearest existing binding, or creates one in the
     * first boundary scope reached. Returns false if neither happened (the caller then defines
     * the name in its own scope).
     */
    bool assignOuter(Symbol name, Value& value) {
      for (Environment* e = this; e; e = e->outer_.get()) {
        if (Value* v = e->lookupOwn(name)) {
          *v = std::move(value);
          return true;
        }
        if (e->boundary_) {
          e->add(name, std::move(value));
          return true;
        }
      }
      return false;
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

    // The first few slots live inside the Environment, so a function call's scope is a single
    // allocation. Larger scopes move to the heap.
    class SlotList {
    public:
      static constexpr size_t kInline = 4;

      SlotList() = default;
      SlotList(const SlotList&) = delete;
      SlotList& operator=(const SlotList&) = delete;
      ~SlotList() {
        for (size_t i = 0; i < size_; ++i) data()[i].~Slot();
        if (data_ != inline_()) ::operator delete(data_);
      }

      size_t size() const { return size_; }
      Slot& operator[](size_t i) { return data_[i]; }
      Slot* begin() { return data_; }
      Slot* end() { return data_ + size_; }

      void reserve(size_t n) {
        if (n > capacity_) grow(n);
      }
      void push_back(Slot slot) {
        if (size_ == capacity_) grow(capacity_ * 2);
        new (data_ + size_) Slot(std::move(slot));
        ++size_;
      }

    private:
      Slot* inline_() { return reinterpret_cast<Slot*>(storage_); }
      Slot* data() { return data_; }
      void grow(size_t n) {
        auto* fresh = static_cast<Slot*>(::operator new(n * sizeof(Slot)));
        for (size_t i = 0; i < size_; ++i) {
          new (fresh + i) Slot(std::move(data_[i]));
          data_[i].~Slot();
        }
        if (data_ != inline_()) ::operator delete(data_);
        data_ = fresh;
        capacity_ = n;
      }

      alignas(Slot) unsigned char storage_[kInline * sizeof(Slot)];
      Slot* data_ = inline_();
      size_t size_ = 0;
      size_t capacity_ = kInline;
    };

    SlotList slots_;
    std::vector<int32_t> index_;  // symbol -> slot, empty while the scope is small
    std::shared_ptr<Environment> outer_;
    bool boundary_ = false;
  };

}  // namespace mni
