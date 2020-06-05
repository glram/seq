#include <algorithm>
#include <iterator>
#include <sstream>

#include "operand.h"
#include "pattern.h"
#include "rvalue.h"

using namespace seq;
using namespace ir;

CallRValue::CallRValue(std::shared_ptr<Operand> func)
    : Rvalue{func->getType()->getRType()}, func{func}, args{} {}

CallRValue::CallRValue(std::shared_ptr<Operand> func,
                       std::vector<std::shared_ptr<Operand>> args)
    : Rvalue{func->getType()->getRType()}, func{func}, args{args} {}

CallRValue::CallRValue(CallRValue &other)
    : Rvalue{func->getType()->getRType()}, func{func}, args{} {
  std::copy(other.args.begin(), other.args.end(),
            std::back_inserter(this->args));
}

std::shared_ptr<Operand> CallRValue::getFunc() const { return func; }

std::vector<std::shared_ptr<Operand>> CallRValue::getArgs() const {
  return args;
}
std::string CallRValue::textRepresentation() const {
  std::stringstream stream;

  stream << func->referenceString() << "(";
  for (auto it = args.begin(); it != args.end(); it++) {
    stream << (*it)->referenceString();
    if (it + 1 != args.end())
      stream << ", ";
  }
  stream << ")";

  return stream.str();
}

OperandRvalue::OperandRvalue(std::shared_ptr<Operand> operand)
    : Rvalue{operand->getType()}, operand{operand} {}

std::shared_ptr<Operand> OperandRvalue::getOperand() const { return operand; }

std::string OperandRvalue::textRepresentation() const {
  return operand->textRepresentation();
}

MatchRvalue::MatchRvalue(std::shared_ptr<Pattern> pattern,
                         std::shared_ptr<Operand> operand)
    : Rvalue{types::kBoolType}, pattern{pattern}, operand{operand} {}

std::shared_ptr<Pattern> MatchRvalue::getPattern() const { return pattern; }

std::shared_ptr<Operand> MatchRvalue::getOperand() const { return operand; }

// TODO
std::string MatchRvalue::textRepresentation() const {
  return "match(" + pattern->textRepresentation() + ", " +
         operand->textRepresentation() + ")";
}

PipelineRvalue::PipelineRvalue(std::vector<std::shared_ptr<Operand>> stages,
                               std::vector<bool> parallel)
    : Rvalue{(stages.size() > 0) ? stages[stages.size() - 1]->getType()
                                 : types::kVoidType},
      stages{stages}, parallel{parallel} {}

PipelineRvalue::PipelineRvalue(PipelineRvalue &other)
    : Rvalue{other.getType()}, stages{}, parallel{parallel} {
  std::copy(other.stages.begin(), other.stages.end(),
            std::back_inserter(this->stages));
}

std::vector<std::shared_ptr<Operand>> PipelineRvalue::getStages() const {
  return stages;
}
std::vector<bool> PipelineRvalue::getParallel() const { return parallel; }
std::string PipelineRvalue::textRepresentation() const {
  std::stringstream stream;
  for (int i = 0; i < stages.size(); i++) {
    stream << stages[i]->textRepresentation();
    if (i + 1 != stages.size()) {
      auto separator = (parallel[i]) ? "||>" : "|>";
      stream << " " << separator << " ";
    }
  }
  return stream.str();
}
