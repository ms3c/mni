#include "stack.h"

#include <cstdint>

#if defined(_WIN32)
#  define NOMINMAX
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#elif defined(__APPLE__) || defined(__linux__)
#  include <pthread.h>
#endif

namespace mni {

  namespace {
    struct StackBounds {
      std::uintptr_t low = 0;  // 0 if unknown
      std::uintptr_t high = 0;
    };

    StackBounds stackBounds() {
      StackBounds bounds;
#if defined(_WIN32)
      ULONG_PTR low = 0, high = 0;
      GetCurrentThreadStackLimits(&low, &high);
      bounds.low = static_cast<std::uintptr_t>(low);
      bounds.high = static_cast<std::uintptr_t>(high);
#elif defined(__APPLE__)
      const pthread_t self = pthread_self();
      bounds.high = reinterpret_cast<std::uintptr_t>(pthread_get_stackaddr_np(self));
      bounds.low = bounds.high - pthread_get_stacksize_np(self);
#elif defined(__linux__)
      pthread_attr_t attr;
      if (pthread_getattr_np(pthread_self(), &attr) == 0) {
        void* address = nullptr;
        std::size_t size = 0;
        if (pthread_attr_getstack(&attr, &address, &size) == 0) {
          bounds.low = reinterpret_cast<std::uintptr_t>(address);
          bounds.high = bounds.low + size;
        }
        pthread_attr_destroy(&attr);
      }
#endif
      return bounds;
    }
  }  // namespace

  // Sanitizers may move local variables to a separate "fake stack" on the heap, so where the
  // compiler allows it the frame address is used to find our position on the real stack.
  std::size_t stackBytesRemaining() {
    constexpr auto kUnknown = static_cast<std::size_t>(-1);
    thread_local const StackBounds bounds = stackBounds();
    if (bounds.low == 0) return kUnknown;

#if defined(__GNUC__) || defined(__clang__)
    const auto here = reinterpret_cast<std::uintptr_t>(__builtin_frame_address(0));
#else
    char marker;
    const auto here = reinterpret_cast<std::uintptr_t>(&marker);
#endif
    // Not on this thread's own stack (custom stacks, fibers): we cannot tell, so do not block.
    if (here < bounds.low || here >= bounds.high) return kUnknown;
    return static_cast<std::size_t>(here - bounds.low);
  }

}  // namespace mni
