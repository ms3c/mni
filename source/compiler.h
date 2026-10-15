#pragma once

// Internal: compiles a parsed program to bytecode. Not installed.

#include <memory>

#include "bytecode.h"

namespace mni {

  /**
   * Compiles `program` to a proto of the given kind: Main for a script or a REPL line, Module for
   * an imported file. The program's syntax tree must outlive the proto (functions point back into
   * it for their names and parameters).
   *
   * The compiler never throws NuruError. Things the tree-walking evaluator only reports when it
   * reaches them are compiled to a Throw instruction, so they happen at the same moment. It throws
   * std::logic_error if it finds a bug in itself (for example an unbalanced operand stack).
   */
  std::unique_ptr<Proto> compileProgram(const Program& program, Proto::Kind kind);

}  // namespace mni
