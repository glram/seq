#include "terminator.h"

#include "util/fmt/format.h"

#include "bblock.h"
#include "operand.h"
#include "var.h"

#include "common/visitor.h"

namespace seq {
namespace ir {

void Terminator::accept(common::IRVisitor &v) { v.visit(getShared()); }

void JumpTerminator::accept(common::IRVisitor &v) {
  v.visit(std::static_pointer_cast<JumpTerminator>(getShared()));
}

std::string JumpTerminator::textRepresentation() const {
  return fmt::format(FMT_STRING("jmp {}; {}"), dst.lock()->referenceString(),
                     attributeString());
}

void CondJumpTerminator::accept(common::IRVisitor &v) {
  v.visit(std::static_pointer_cast<CondJumpTerminator>(getShared()));
}

std::string CondJumpTerminator::textRepresentation() const {
  return fmt::format(FMT_STRING("condjump ({}) {} {}; {}"), cond->textRepresentation(),
                     tDst.lock()->referenceString(), fDst.lock()->referenceString(),
                     attributeString());
}

void ReturnTerminator::accept(common::IRVisitor &v) {
  v.visit(std::static_pointer_cast<ReturnTerminator>(getShared()));
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

void YieldTerminator::accept(common::IRVisitor &v) {
  v.visit(std::static_pointer_cast<YieldTerminator>(getShared()));
}

std::string YieldTerminator::textRepresentation() const {
  fmt::memory_buffer buf;

  fmt::format_to(buf, FMT_STRING("yield"));
  if (res) {
    fmt::format_to(buf, FMT_STRING(" {}"), res->textRepresentation());
  }
  if (inVar) {
    fmt::format_to(buf, FMT_STRING(" -> {}"), inVar->referenceString());
  }

  fmt::format_to(buf, FMT_STRING(" {}; {}"), dst.lock()->referenceString(),
                 attributeString());

  return std::string(buf.data(), buf.size());
}

void ThrowTerminator::accept(common::IRVisitor &v) {
  v.visit(std::static_pointer_cast<ThrowTerminator>(getShared()));
}

std::string ThrowTerminator::textRepresentation() const {
  return fmt::format(FMT_STRING("throw ({}); {}"), operand->textRepresentation(),
                     attributeString());
}

void AssertTerminator::accept(common::IRVisitor &v) {
  v.visit(std::static_pointer_cast<AssertTerminator>(getShared()));
}

std::string AssertTerminator::textRepresentation() const {
  return fmt::format(FMT_STRING("assert ({}) {}; {}"), operand->textRepresentation(),
                     dst.lock()->referenceString(), attributeString());
}

} // namespace ir
} // namespace seq
