#pragma once

#include <memory>

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
  Rvalue(std::shared_ptr<types::Type> type) : type{type} {};

  std::shared_ptr<types::Type> getType() const { return type; };

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
  CallRValue(CallRValue &other);

  std::shared_ptr<Operand> getFunc() const;
  std::vector<std::shared_ptr<Operand>> getArgs() const;

  std::string textRepresentation() const override;
};

class OperandRvalue : public Rvalue {
private:
  std::shared_ptr<Operand> operand;

public:
  explicit OperandRvalue(std::shared_ptr<Operand> operand);

  std::shared_ptr<Operand> getOperand() const;

  std::string textRepresentation() const override;
};

class MatchRvalue : public Rvalue {
private:
  std::shared_ptr<Pattern> pattern;
  std::shared_ptr<Operand> operand;

public:
  MatchRvalue(std::shared_ptr<Pattern> pattern,
              std::shared_ptr<Operand> operand);

  std::shared_ptr<Pattern> getPattern() const;
  std::shared_ptr<Operand> getOperand() const;

  std::string textRepresentation() const override;
};

class PipelineRvalue : public Rvalue {
private:
  std::vector<std::shared_ptr<Operand>> stages;
  std::vector<bool> parallel;

public:
  PipelineRvalue(std::vector<std::shared_ptr<Operand>> stages,
                 std::vector<bool> parallel);
  PipelineRvalue(PipelineRvalue &other);

  std::vector<std::shared_ptr<Operand>> getStages() const;
  std::vector<bool> getParallel() const;

  std::string textRepresentation() const override;
};

} // namespace ir
} // namespace seq