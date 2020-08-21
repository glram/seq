#pragma once

#include <memory>
#include <utility>

#include "base.h"
#include "types/types.h"

namespace seq {
namespace ir {

class Operand;
class Pattern;

class Rvalue : public AttributeHolder<Rvalue> {
private:
  std::shared_ptr<types::Type> type;

public:
  explicit Rvalue(std::shared_ptr<types::Type> type) : type(std::move(type)){};

  std::shared_ptr<types::Type> getType() { return type; };

  std::string referenceString() const override { return "rvalue"; };
};

class CallRValue : public Rvalue {
private:
  std::shared_ptr<Operand> func;
  std::vector<std::shared_ptr<Operand>> args;

public:
  explicit CallRValue(std::shared_ptr<Operand> func);
  CallRValue(std::shared_ptr<Operand> func,
             std::vector<std::shared_ptr<Operand>> args);

  std::shared_ptr<Operand> getFunc() { return func; }
  std::vector<std::shared_ptr<Operand>> getArgs() { return args; }

  std::string textRepresentation() const override;
};

class OperandRvalue : public Rvalue {
private:
  std::shared_ptr<Operand> operand;

public:
  explicit OperandRvalue(std::shared_ptr<Operand> operand);

  std::shared_ptr<Operand> getOperand() { return operand; }

  std::string textRepresentation() const override;
};

class MatchRvalue : public Rvalue {
private:
  std::shared_ptr<Pattern> pattern;
  std::shared_ptr<Operand> operand;

public:
  MatchRvalue(std::shared_ptr<Pattern> pattern,
              std::shared_ptr<Operand> operand)
      : Rvalue(types::kBoolType), pattern(std::move(pattern)),
        operand(std::move(operand)) {}

  std::shared_ptr<Pattern> getPattern() { return pattern; }
  std::shared_ptr<Operand> getOperand() { return operand; }

  std::string textRepresentation() const override;
};

class PipelineRvalue : public Rvalue {
private:
  std::vector<std::shared_ptr<Operand>> stages;
  std::vector<bool> parallel;

public:
  PipelineRvalue(std::vector<std::shared_ptr<Operand>> stages,
                 std::vector<bool> parallel);

  std::vector<std::shared_ptr<Operand>> getStages() { return stages; }
  std::vector<bool> getParallel() const { return parallel; }

  std::string textRepresentation() const override;
};

} // namespace ir
} // namespace seq