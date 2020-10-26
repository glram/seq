#include "util/fmt/format.h"

#include "common/visitor.h"

#include "bblock.h"
#include "instr.h"
#include "terminator.h"
#include "trycatch.h"

namespace seq {
namespace ir {

int BasicBlock::currentId = 0;

void BasicBlock::resetId() { currentId = 0; }

std::string BasicBlock::referenceString() const {
  return fmt::format(FMT_STRING("bb#{}"), id);
}

void BasicBlock::accept(common::IRVisitor &v) { v.visit(getShared()); }

std::shared_ptr<TryCatch> BasicBlock::getHandlerTryCatch() {
  return isCatch ? tc->getParent().lock() : tc;
}

std::shared_ptr<TryCatch> BasicBlock::getFinallyTryCatch() { return tc; }

std::string BasicBlock::textRepresentation() const {
  fmt::memory_buffer buf;
  fmt::format_to(buf, FMT_STRING("{}"), referenceString());
  if (tc)
    fmt::format_to(buf, FMT_STRING("({})"), tc->referenceString());
  buf.push_back('\n');
  buf.push_back('{');
  for (const auto &instrPtr : instructions) {
    fmt::format_to(buf, FMT_STRING("{};\n"), instrPtr->textRepresentation());
  }
  if (terminator)
    fmt::format_to(buf, "{};\n}}; {}", terminator->textRepresentation(),
                   attributeString());
  else
    fmt::format_to(buf, "noterm;\n}}; {}", attributeString());

  return std::string(buf.data(), buf.size());
}

} // namespace ir
} // namespace seq
