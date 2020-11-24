#include "rvalue.h"

#include <utility>

#include "operand.h"
#include "pattern.h"
#include "var.h"

#include "common/visitor.h"

namespace seq {
namespace ir {

void Rvalue::accept(common::SIRVisitor &v) { v.visit(getShared()); }

void MemberRvalue::accept(common::SIRVisitor &v) { v.visit(getShared<MemberRvalue>()); }

std::shared_ptr<types::Type> MemberRvalue::rvalType() {
  auto type = std::static_pointer_cast<types::MemberedType>(var->getType())
                  ->getMemberType(field);
  return type;
}

std::string MemberRvalue::textRepresentation() const {
  return fmt::format(FMT_STRING("({}).{}"), var->textRepresentation(), field);
}

void CallRvalue::accept(common::SIRVisitor &v) { v.visit(getShared<CallRvalue>()); }

std::shared_ptr<types::Type> CallRvalue::rvalType() {
  return std::static_pointer_cast<types::FuncType>(func->getType())->getRType();
}

std::string CallRvalue::textRepresentation() const {
  fmt::memory_buffer buf;
  fmt::format_to(buf, FMT_STRING("{}("), func->textRepresentation());
  for (auto it = args.begin(); it != args.end(); it++) {
    fmt::format_to(buf, FMT_STRING("{}"), (*it)->textRepresentation());
    if (it + 1 != args.end())
      fmt::format_to(buf, FMT_STRING(", "));
  }
  buf.push_back(')');
  return std::string(buf.data(), buf.size());
}

void PartialCallRvalue::accept(common::SIRVisitor &v) {
  v.visit(getShared<PartialCallRvalue>());
}

std::string PartialCallRvalue::textRepresentation() const {
  fmt::memory_buffer buf;
  fmt::format_to(buf, FMT_STRING("{}("), func->textRepresentation());
  for (auto it = args.begin(); it != args.end(); it++) {
    fmt::format_to(buf, FMT_STRING("{}"), *it ? (*it)->textRepresentation() : "...");
    if (it + 1 != args.end())
      fmt::format_to(buf, FMT_STRING(", "));
  }
  buf.push_back(')');
  return std::string(buf.data(), buf.size());
}

void OperandRvalue::accept(common::SIRVisitor &v) {
  v.visit(getShared<OperandRvalue>());
}

std::shared_ptr<types::Type> OperandRvalue::rvalType() { return operand->getType(); }

std::string OperandRvalue::textRepresentation() const {
  return operand->textRepresentation();
}

void MatchRvalue::accept(common::SIRVisitor &v) { v.visit(getShared<MatchRvalue>()); }

std::string MatchRvalue::textRepresentation() const {
  return fmt::format(FMT_STRING("match({}, {})"), pattern->textRepresentation(),
                     operand->textRepresentation());
}

void PipelineRvalue::accept(common::SIRVisitor &v) {
  v.visit(getShared<PipelineRvalue>());
}

std::string PipelineRvalue::textRepresentation() const {
  fmt::memory_buffer buf;
  for (int i = 0; i < stages.size(); i++) {
    fmt::format_to(buf, FMT_STRING("{}"), stages[i]->textRepresentation());
    if (i + 1 != stages.size()) {
      fmt::format_to(buf, FMT_STRING("{}"), (parallel[i]) ? "||>" : "|>");
    }
  }
  return std::string(buf.data(), buf.size());
}

void StackAllocRvalue::accept(common::SIRVisitor &v) {
  v.visit(getShared<StackAllocRvalue>());
}

std::string StackAllocRvalue::textRepresentation() const {
  return fmt::format(FMT_STRING("new({}, {})"), tval->textRepresentation(), count);
}

} // namespace ir
} // namespace seq
