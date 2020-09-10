#include "util/fmt/format.h"

#include "bblock.h"
#include "operand.h"
#include "terminator.h"
#include "var.h"

namespace seq {
namespace ir {

std::string JumpTerminator::textRepresentation() const {
  return fmt::format(FMT_STRING("jmp {}; {}"), dst.lock()->referenceString(),
                     attributeString());
}

std::string CondJumpTerminator::textRepresentation() const {
  return fmt::format(FMT_STRING("condjump ({}) {} {}; {}"), cond->textRepresentation(),
                     tDst.lock()->referenceString(), fDst.lock()->referenceString(),
                     attributeString());
}

std::string ReturnTerminator::textRepresentation() const {
  fmt::memory_buffer buf;

  fmt::format_to(buf, FMT_STRING("return"));
  if (operand) {
    fmt::format_to(buf, FMT_STRING(" {}"), operand->textRepresentation());
  }
  fmt::format_to(buf, FMT_STRING("; {}"), attributeString());
  return std::string(buf.data(), buf.size());
}

std::string YieldTerminator::textRepresentation() const {
  fmt::memory_buffer buf;

  fmt::format_to(buf, FMT_STRING("yield"));
  if (res) {
    fmt::format_to(buf, FMT_STRING(" {}"), res->textRepresentation());
  }
  if (inVar.lock()) {
    fmt::format_to(buf, FMT_STRING(" -> {}"), inVar.lock()->referenceString());
  }

  fmt::format_to(buf, FMT_STRING(" {}; {}"), dst.lock()->referenceString(),
                 attributeString());

  return std::string(buf.data(), buf.size());
}

std::string ThrowTerminator::textRepresentation() const {
  return fmt::format(FMT_STRING("throw ({}); {}"), operand->textRepresentation(),
                     attributeString());
}

std::string AssertTerminator::textRepresentation() const {
  return fmt::format(FMT_STRING("assert ({}) {}; {}"), operand->textRepresentation(),
                     dst.lock()->referenceString(), attributeString());
}

} // namespace ir
} // namespace seq
