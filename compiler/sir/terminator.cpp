#include "terminator.h"

#include "util/fmt/format.h"

#include "bblock.h"
#include "operand.h"
#include "var.h"

#include "common/visitor.h"

namespace seq {
namespace ir {

void Terminator::accept(common::SIRVisitor &v) { v.visit(getShared()); }

void JumpTerminator::accept(common::SIRVisitor &v) {
  v.visit(getShared<JumpTerminator>());
}

std::string JumpTerminator::textRepresentation() const {
  return fmt::format(FMT_STRING("jmp {}; {}"), dst.lock()->referenceString(),
                     attributeString());
}

void CondJumpTerminator::accept(common::SIRVisitor &v) {
  v.visit(getShared<CondJumpTerminator>());
}

std::string CondJumpTerminator::textRepresentation() const {
  return fmt::format(FMT_STRING("condjump ({}) {} {}; {}"), cond->textRepresentation(),
                     tDst.lock()->referenceString(), fDst.lock()->referenceString(),
                     attributeString());
}

void ReturnTerminator::accept(common::SIRVisitor &v) {
  v.visit(getShared<ReturnTerminator>());
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

void YieldTerminator::accept(common::SIRVisitor &v) {
  v.visit(getShared<YieldTerminator>());
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

void ThrowTerminator::accept(common::SIRVisitor &v) {
  v.visit(getShared<ThrowTerminator>());
}

std::string ThrowTerminator::textRepresentation() const {
  return fmt::format(FMT_STRING("throw ({}); {}"), operand->textRepresentation(),
                     attributeString());
}

void AssertTerminator::accept(common::SIRVisitor &v) {
  v.visit(getShared<AssertTerminator>());
}

std::string AssertTerminator::textRepresentation() const {
  return fmt::format(FMT_STRING("assert ({}, \"{}\") {}; {}"),
                     operand->textRepresentation(), msg, dst.lock()->referenceString(),
                     attributeString());
}

void FinallyTerminator::accept(common::SIRVisitor &v) {
  v.visit(getShared<FinallyTerminator>());
}

std::string FinallyTerminator::textRepresentation() const {
  return fmt::format(FMT_STRING("end_finally ({}); {}"), tc->referenceString(),
                     attributeString());
}

} // namespace ir
} // namespace seq
