#include "util/fmt/format.h"

#include "operand.h"
#include "pattern.h"
#include "rvalue.h"

#include "var.h"
#include <utility>

namespace seq {
namespace ir {

MemberRvalue::MemberRvalue(std::shared_ptr<Operand> var, std::string field)
    : var(std::move(var)), field(std::move(field)) {}

std::shared_ptr<types::Type> MemberRvalue::getType() { return var->getType(); }

std::string MemberRvalue::textRepresentation() const {
  return fmt::format(FMT_STRING("{}.{}"), var->textRepresentation(), field);
}

CallRvalue::CallRvalue(std::shared_ptr<Operand> func) : func(std::move(func)) {}

CallRvalue::CallRvalue(std::shared_ptr<Operand> func,
                       std::vector<std::shared_ptr<Operand>> args)
    : func(std::move(func)), args(std::move(args)) {}

std::shared_ptr<types::Type> CallRvalue::getType() {
  return std::static_pointer_cast<types::FuncType>(func->getType())->getRType();
}

PartialCallRvalue::PartialCallRvalue(std::shared_ptr<Operand> func,
                                     std::vector<std::shared_ptr<Operand>> args,
                                     std::shared_ptr<types::PartialFuncType> tval)
    : func(std::move(func)), args(std::move(args)), tval(std::move(tval)) {}

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

OperandRvalue::OperandRvalue(std::shared_ptr<Operand> operand)
    : operand(std::move(operand)) {}

std::shared_ptr<types::Type> OperandRvalue::getType() { return operand->getType(); }

std::string OperandRvalue::textRepresentation() const {
  return operand->textRepresentation();
}

std::string MatchRvalue::textRepresentation() const {
  return fmt::format(FMT_STRING("match({}, {})"), pattern->textRepresentation(),
                     operand->textRepresentation());
}

PipelineRvalue::PipelineRvalue(std::vector<std::shared_ptr<Operand>> stages,
                               std::vector<bool> parallel)
    : stages(std::move(stages)), parallel(std::move(parallel)) {}

std::shared_ptr<types::Type> PipelineRvalue::getType() {
  return !stages.empty() ? stages[stages.size() - 1]->getType() : types::kVoidType;
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

std::string StackAllocRvalue::textRepresentation() const {
  return fmt::format(FMT_STRING("new({}, {})"), tval->textRepresentation(),
                     count->textRepresentation());
}

} // namespace ir
} // namespace seq
