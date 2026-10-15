#pragma once

#include <cstddef>

namespace mni {

  /**
   * Approximate bytes of stack left on the calling thread, or SIZE_MAX if the platform cannot say.
   * The interpreter uses it to turn runaway recursion into an error instead of a crash; call depth
   * alone is not enough, because default stack sizes differ a lot (8 MB on Linux and macOS, 1 MB
   * on Windows) and debug frames are large.
   */
  std::size_t stackBytesRemaining();

}  // namespace mni
