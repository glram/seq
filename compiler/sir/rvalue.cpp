#include "util/fmt/format.h"

#include "operand.h"
#include "pattern.h"
#include "rvalue.h"

namespace seq {
namespace ir {

CallRValue::CallRValue(std::shared_ptr<Operand> func)
    : Rvalue(func->getType()->getRType()), func(std::move(func)) {}

CallRValue::CallRValue(std::shared_ptr<Operand> func,
                       std::vector<std::shared_ptr<Operand>> args)
    : Rvalue(func->getType()->getRType()), func(std::move(func)),
      args(std::move(args)) {}

std::string CallRValue::textRepresentation() const {
  fmt::memory_buffer buf;
  fmt::format_to(buf, FMT_STRING("{}("), func->referenceString());
  for (auto it = args.begin(); it != args.end(); it++) {
    fmt::format_to(buf, FMT_STRING("{}"), (*it)->referenceString());
    if (it + 1 != args.end())
      fmt::format_to(buf, FMT_STRING(", "));
  }
  buf.push_back(')');
  return std::string(buf.data(), buf.size());
}

OperandRvalue::OperandRvalue(std::shared_ptr<Operand> operand)
    : Rvalue(operand->getType()), operand(std::move(operand)) {}

std::string OperandRvalue::textRepresentation() const {
  return operand->textRepresentation();
}

std::string MatchRvalue::textRepresentation() const {
  return fmt::format(FMT_STRING("match({}, {})"), pattern->textRepresentation(),
                     operand->textRepresentation());
}

PipelineRvalue::PipelineRvalue(std::vector<std::shared_ptr<Operand>> stages,
                               std::vector<bool> parallel)
    : Rvalue(!stages.empty() ? stages[stages.size() - 1]->getType()
                             : types::kVoidType),
      stages(stages), parallel(std::move(parallel)) {}

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

} // namespace ir
} // namespace seq
