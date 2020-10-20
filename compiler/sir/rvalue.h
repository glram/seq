#pragma once

#include <memory>
#include <utility>

#include "base.h"
#include "types/types.h"

namespace seq {
namespace ir {

namespace common {
class IRVisitor;
}

class Operand;
class Pattern;
class Var;

class Rvalue : public AttributeHolder<Rvalue> {
public:
  virtual ~Rvalue() = default;

  virtual void accept(common::IRVisitor &v);

  virtual std::shared_ptr<types::Type> getType() = 0;

  std::string referenceString() const override { return "rvalue"; };
};

class MemberRvalue : public Rvalue {
private:
  std::shared_ptr<Operand> var;
  std::string field;

public:
  MemberRvalue(std::shared_ptr<Operand> var, std::string field);

  void accept(common::IRVisitor &v) override;

  std::shared_ptr<types::Type> getType() override;

  std::shared_ptr<Operand> getVar() { return var; }
  std::string getField() const { return field; }

  std::string textRepresentation() const override;
};

class CallRvalue : public Rvalue {
private:
  std::shared_ptr<Operand> func;
  std::vector<std::shared_ptr<Operand>> args;

public:
  explicit CallRvalue(std::shared_ptr<Operand> func);
  CallRvalue(std::shared_ptr<Operand> func, std::vector<std::shared_ptr<Operand>> args);

  void accept(common::IRVisitor &v) override;

  std::shared_ptr<types::Type> getType() override;

  std::shared_ptr<Operand> getFunc() { return func; }
  std::vector<std::shared_ptr<Operand>> getArgs() { return args; }

  std::string textRepresentation() const override;
};

class PartialCallRvalue : public Rvalue {
private:
  std::shared_ptr<Operand> func;
  std::vector<std::shared_ptr<Operand>> args;
  std::shared_ptr<types::PartialFuncType> tval;

public:
  PartialCallRvalue(std::shared_ptr<Operand> func,
                    std::vector<std::shared_ptr<Operand>> args,
                    std::shared_ptr<types::PartialFuncType> tval);

  void accept(common::IRVisitor &v) override;

  std::shared_ptr<types::Type> getType() override { return tval; };

  std::shared_ptr<Operand> getFunc() { return func; }
  std::vector<std::shared_ptr<Operand>> getArgs() { return args; }

  std::string textRepresentation() const override;
};

class OperandRvalue : public Rvalue {
private:
  std::shared_ptr<Operand> operand;

public:
  explicit OperandRvalue(std::shared_ptr<Operand> operand);

  void accept(common::IRVisitor &v) override;

  std::shared_ptr<types::Type> getType() override;

  std::shared_ptr<Operand> getOperand() { return operand; }

  std::string textRepresentation() const override;
};

class MatchRvalue : public Rvalue {
private:
  std::shared_ptr<Pattern> pattern;
  std::shared_ptr<Operand> operand;

public:
  MatchRvalue(std::shared_ptr<Pattern> pattern, std::shared_ptr<Operand> operand)
      : pattern(std::move(pattern)), operand(std::move(operand)) {}

  void accept(common::IRVisitor &v) override;

  std::shared_ptr<types::Type> getType() override { return types::kBoolType; }

  std::shared_ptr<Pattern> getPattern() { return pattern; }
  std::shared_ptr<Operand> getOperand() { return operand; }

  std::string textRepresentation() const override;
};

class PipelineRvalue : public Rvalue {
private:
  std::vector<std::shared_ptr<Operand>> stages;
  std::vector<bool> parallel;
  std::vector<std::shared_ptr<types::Type>> inTypes;
  std::shared_ptr<types::Type> outType;

public:
  PipelineRvalue(std::vector<std::shared_ptr<Operand>> stages,
                 std::vector<bool> parallel,
                 std::vector<std::shared_ptr<types::Type>> inTypes,
                 std::shared_ptr<types::Type> outType);

  void accept(common::IRVisitor &v) override;

  std::shared_ptr<types::Type> getType() override { return outType; };
  std::vector<std::shared_ptr<types::Type>> getInTypes() { return inTypes; }

  std::vector<std::shared_ptr<Operand>> getStages() { return stages; }
  std::vector<bool> getParallel() const { return parallel; }

  std::string textRepresentation() const override;
};

class StackAllocRvalue : public Rvalue {
private:
  std::shared_ptr<types::Array> tval;
  uint64_t count;

public:
  StackAllocRvalue(std::shared_ptr<types::Array> tval, uint64_t count)
      : tval(std::move(tval)), count(count) {}

  void accept(common::IRVisitor &v) override;

  std::shared_ptr<types::Type> getType() override { return tval; }
  uint64_t getCount() const { return count; }

  std::string textRepresentation() const override;
};

} // namespace ir
} // namespace seq