#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace mni {

  /** An interned name: comparing two identifiers is comparing two integers. */
  using Symbol = std::uint32_t;

  /** The id for `name`, created on first use. Thread-safe. */
  Symbol intern(std::string_view name);

  /** The text of an interned symbol. */
  const std::string& symbolName(Symbol symbol);

  /** How many symbols exist; ids are 0 to symbolCount() - 1. */
  std::size_t symbolCount();

}  // namespace mni
