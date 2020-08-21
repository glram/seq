#include "util/fmt/format.h"

#include "bblock.h"
#include "instr.h"
#include "terminator.h"

namespace seq {
namespace ir {

std::string BasicBlock::referenceString() const {
  return fmt::format(FMT_STRING("bb#{}"), id);
}

std::string BasicBlock::textRepresentation() const {
  fmt::memory_buffer buf;
  fmt::format_to(buf, FMT_STRING("{} {{"), referenceString());
  for (const auto &instrPtr : instructions) {
    fmt::format_to(buf, FMT_STRING("{};\n"), instrPtr->textRepresentation());
  }
  fmt::format_to(buf, "{};\n}}; {}", terminator->textRepresentation(),
                 attributeString());
  return std::string(buf.data(), buf.size());
}

} // namespace ir
} // namespace seq
