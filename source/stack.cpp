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
    // Lowest address of the calling thread's stack, or 0 if unknown.
    std::uintptr_t stackLow() {
#if defined(_WIN32)
      ULONG_PTR low = 0, high = 0;
      GetCurrentThreadStackLimits(&low, &high);
      return static_cast<std::uintptr_t>(low);
#elif defined(__APPLE__)
      const pthread_t self = pthread_self();
      return reinterpret_cast<std::uintptr_t>(pthread_get_stackaddr_np(self))
             - pthread_get_stacksize_np(self);
#elif defined(__linux__)
      pthread_attr_t attr;
      if (pthread_getattr_np(pthread_self(), &attr) != 0) return 0;
      void* address = nullptr;
      std::size_t size = 0;
      const int status = pthread_attr_getstack(&attr, &address, &size);
      pthread_attr_destroy(&attr);
      return status == 0 ? reinterpret_cast<std::uintptr_t>(address) : 0;
#else
      return 0;
#endif
    }
  }  // namespace

  std::size_t stackBytesRemaining() {
    thread_local const std::uintptr_t low = stackLow();
    if (low == 0) return static_cast<std::size_t>(-1);
    char marker;
    const auto here = reinterpret_cast<std::uintptr_t>(&marker);
    return here > low ? static_cast<std::size_t>(here - low) : 0;
  }

}  // namespace mni
