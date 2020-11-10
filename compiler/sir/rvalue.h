#pragma once

#include <memory>
#include <utility>

#include "base.h"
#include "types/types.h"

namespace seq {
namespace ir {

namespace common {
class SIRVisitor;
}

class Operand;
class Pattern;
class Var;

/// SIR object representing the right hand side of an assignment.
class Rvalue : public AttributeHolder<Rvalue> {
private:
  virtual std::shared_ptr<types::Type> rvalType() = 0;

public:
  virtual ~Rvalue() = default;

  virtual void accept(common::SIRVisitor &v);

  /// @tparam the expected type of SIR "type"
  /// @return the type of the rvalue
  template <typename Type = types::Type> std::shared_ptr<Type> getType() {
    return std::static_pointer_cast<Type>(rvalType());
  }

  std::string referenceString() const override { return "rvalue"; };
};

/// Rvalue representing the member of an operand.
class MemberRvalue : public Rvalue {
private:
  /// the "variable"
  std::shared_ptr<Operand> var;
  /// the field
  std::string field;

  std::shared_ptr<types::Type> rvalType() override;

public:
  /// Constructs a member rvalue.
  /// @param the var
  /// @param the field
  MemberRvalue(std::shared_ptr<Operand> var, std::string field)
      : var(std::move(var)), field(std::move(field)) {}

  void accept(common::SIRVisitor &v) override;

  /// @return the variable
  std::shared_ptr<Operand> getVar() { return var; }

  /// @return the field
  std::string getField() const { return field; }

  std::string textRepresentation() const override;
};

/// Rvalue representing a function call.
class CallRvalue : public Rvalue {
private:
  /// the function
  std::shared_ptr<Operand> func;
  /// the arguments
  std::vector<std::shared_ptr<Operand>> args;

  std::shared_ptr<types::Type> rvalType() override;

public:
  /// Constructs a call rvalue.
  /// @param func the function
  /// @param args the arguments
  CallRvalue(std::shared_ptr<Operand> func, std::vector<std::shared_ptr<Operand>> args)
      : func(std::move(func)), args(std::move(args)) {}

  /// Constructs a call rvalue with no arguments.
  /// @param func the function
  explicit CallRvalue(std::shared_ptr<Operand> func)
      : CallRvalue(std::move(func), {}) {}

  void accept(common::SIRVisitor &v) override;

  /// @return the function
  std::shared_ptr<Operand> getFunc() { return func; }

  /// @return the arguments
  std::vector<std::shared_ptr<Operand>> getArgs() { return args; }

  std::string textRepresentation() const override;
};

/// Rvalue representing a partial function call.
class PartialCallRvalue : public Rvalue {
private:
  /// the "function"
  std::shared_ptr<Operand> func;
  /// the arguments, nullptr if not specified in partial call
  std::vector<std::shared_ptr<Operand>> args;
  /// the type of the partial call result
  std::shared_ptr<types::PartialFuncType> tval;

  std::shared_ptr<types::Type> rvalType() override { return tval; }

public:
  /// Constructs a partial call rvalue.
  /// @param func the "function"
  /// @param args the arguments, may contain nullptr
  /// @param tval the type of the partial call result
  PartialCallRvalue(std::shared_ptr<Operand> func,
                    std::vector<std::shared_ptr<Operand>> args,
                    std::shared_ptr<types::PartialFuncType> tval)
      : func(std::move(func)), args(std::move(args)), tval(std::move(tval)) {}

  void accept(common::SIRVisitor &v) override;

  /// @return the function
  std::shared_ptr<Operand> getFunc() { return func; }

  /// @return the arguments
  std::vector<std::shared_ptr<Operand>> getArgs() { return args; }

  std::string textRepresentation() const override;
};

/// Rvalue representing an operand. May be a literal, variable value, etc.
class OperandRvalue : public Rvalue {
private:
  /// the operand
  std::shared_ptr<Operand> operand;

  std::shared_ptr<types::Type> rvalType() override;

public:
  /// Constructs an operand rvalue.
  /// @param operand the operand
  explicit OperandRvalue(std::shared_ptr<Operand> operand)
      : operand(std::move(operand)) {}

  void accept(common::SIRVisitor &v) override;

  /// @return the operand
  std::shared_ptr<Operand> getOperand() { return operand; }

  std::string textRepresentation() const override;
};

/// Rvalue representing an individual pattern match.
class MatchRvalue : public Rvalue {
private:
  /// the pattern
  std::shared_ptr<Pattern> pattern;
  /// the operand being matched
  std::shared_ptr<Operand> operand;

  std::shared_ptr<types::Type> rvalType() override { return types::kBoolType; }

public:
  /// Constructs a match rvalue.
  /// @param pattern the pattern
  /// @param operand the operand being matched
  MatchRvalue(std::shared_ptr<Pattern> pattern, std::shared_ptr<Operand> operand)
      : pattern(std::move(pattern)), operand(std::move(operand)) {}

  void accept(common::SIRVisitor &v) override;

  /// @return the pattern
  std::shared_ptr<Pattern> getPattern() { return pattern; }

  /// @return the operand being matched
  std::shared_ptr<Operand> getOperand() { return operand; }

  std::string textRepresentation() const override;
};

/// Rvalue representing a pipeline
class PipelineRvalue : public Rvalue {
private:
  /// the pipeline stages
  std::vector<std::shared_ptr<Operand>> stages;
  /// whether the stages are parallel
  std::vector<bool> parallel;
  /// the intermediate types of every stage
  std::vector<std::shared_ptr<types::Type>> inTypes;
  /// the output type of the pipeline
  std::shared_ptr<types::Type> outType;

  std::shared_ptr<types::Type> rvalType() override { return outType; }

public:
  /// Constructs a pipeline rvalue.
  /// @param stages the pipeline stages
  /// @param parallel whether the stages are parallel
  /// @param inTypes the intermediate types of every stage
  /// @param outType the output type of the pipeline
  PipelineRvalue(std::vector<std::shared_ptr<Operand>> stages,
                 std::vector<bool> parallel,
                 std::vector<std::shared_ptr<types::Type>> inTypes,
                 std::shared_ptr<types::Type> outType)
      : stages(std::move(stages)), parallel(std::move(parallel)),
        inTypes(std::move(inTypes)), outType(std::move(outType)) {}

  void accept(common::SIRVisitor &v) override;

  /// @return the intermeidate types of every stage
  std::vector<std::shared_ptr<types::Type>> getInTypes() { return inTypes; }

  /// @return the pipeline stages
  std::vector<std::shared_ptr<Operand>> getStages() { return stages; }

  /// @return a vector that contains true if given stage is parallel, false otherwise
  std::vector<bool> getParallel() const { return parallel; }

  std::string textRepresentation() const override;
};

/// Rvalue representing allocating an array on the stack.
class StackAllocRvalue : public Rvalue {
private:
  /// the array type
  std::shared_ptr<types::ArrayType> tval;
  /// number of elements to allocate
  uint64_t count;

  std::shared_ptr<types::Type> rvalType() override { return tval; }

public:
  /// Constructs a stack allocation rvalue.
  /// @param tval the type of the array
  /// @param count the number of elements
  StackAllocRvalue(std::shared_ptr<types::ArrayType> tval, uint64_t count)
      : tval(std::move(tval)), count(count) {}

  void accept(common::SIRVisitor &v) override;

  /// @return the number of elements to allocate
  uint64_t getCount() const { return count; }

  std::string textRepresentation() const override;
};

} // namespace ir
} // namespace seq