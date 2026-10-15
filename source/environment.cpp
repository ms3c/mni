#include <mni/environment.h>

#include <algorithm>
#include <cstddef>
#include <new>

namespace mni {

  namespace {
    // A per-thread free list of fixed-size blocks. A call's scope is allocated and freed in
    // strict rhythm, so recycling the blocks is much cheaper than going through malloc each time.
    struct Node {
      Node* next;
    };

    // Plain thread_local data is cheap to reach; the cleanup object (which has a destructor) is
    // only touched when a fresh block is needed.
    thread_local Node* freeHead = nullptr;

    struct Cleanup {
      ~Cleanup() {
        while (freeHead) {
          Node* next = freeHead->next;
          ::operator delete(freeHead);
          freeHead = next;
        }
      }
    };

    void registerCleanup() {
      thread_local Cleanup cleanup;
      (void)cleanup;
    }

    template <typename T> struct PoolAllocator {
      using value_type = T;
      PoolAllocator() = default;
      template <typename U> PoolAllocator(const PoolAllocator<U>&) {}

      T* allocate(size_t n) {
        if (n == 1 && freeHead) {
          Node* node = freeHead;
          freeHead = node->next;
          return reinterpret_cast<T*>(node);
        }
        registerCleanup();
        return static_cast<T*>(
            ::operator new(n * sizeof(T) < sizeof(Node) ? sizeof(Node) : n * sizeof(T)));
      }
      void deallocate(T* p, size_t n) {
        if (n == 1 && sizeof(T) >= sizeof(Node)) {
          auto* node = reinterpret_cast<Node*>(p);
          node->next = freeHead;
          freeHead = node;
          return;
        }
        ::operator delete(p);
      }
      template <typename U> bool operator==(const PoolAllocator<U>&) const { return true; }
    };
  }  // namespace

  std::shared_ptr<Environment> Environment::make(std::shared_ptr<Environment> outer,
                                                 size_t expected) {
    return std::allocate_shared<Environment>(PoolAllocator<Environment>(), std::move(outer),
                                             expected);
  }

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
