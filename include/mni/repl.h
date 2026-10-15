#pragma once

#include <mni/interpreter.h>

#include <iostream>

namespace mni {

  /**
   * @brief Interactive loop: reads a line (more lines while brackets are open), runs it in
   * `interp`'s persistent scope and prints the result if it is not tupu.
   *
   * Errors are printed to `err` and the loop continues.
   * @return the exit code: 0 at end of input, or the argument of toka()/exit()
   */
  int runRepl(Interpreter& interp, std::istream& in, std::ostream& out, std::ostream& err);

}  // namespace mni
