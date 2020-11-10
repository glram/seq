#pragma once

#include <memory>
#include <string>

#include "base.h"

namespace seq {
namespace ir {

namespace common {
class SIRVisitor;
}

class Lvalue;
class Rvalue;

/// SIR object representing an "instruction," or discrete operation in the context of a
/// basic block.
class Instr : public AttributeHolder<Instr> {
public:
  virtual ~Instr() = default;

  virtual void accept(common::SIRVisitor &v);
  std::string referenceString() const override { return "instr"; };
};

/// SIR instruction representing setting a memory location.
class AssignInstr : public Instr {
private:
  /// the left-hand side
  std::shared_ptr<Lvalue> left;
  /// the right-hand side
  std::shared_ptr<Rvalue> right;

public:
  /// Constructs an assign instruction.
  /// @param left the left-hand side
  /// @param right the right-hand side
  explicit AssignInstr(std::shared_ptr<Lvalue> left, std::shared_ptr<Rvalue> right)
      : left(std::move(left)), right(std::move(right)) {}

  void accept(common::SIRVisitor &v) override;

  /// @return the right-hand side
  std::shared_ptr<Rvalue> getRhs() const { return right; }

  /// @return the left-hand side
  std::shared_ptr<Lvalue> getLhs() const { return left; }

  std::string textRepresentation() const override;
};

/// SIR instruction representing an operation where the rvalue is discarded.
class RvalueInstr : public Instr {
  /// the rvalue
  std::shared_ptr<Rvalue> rvalue;

public:
  /// Constructs an rvalue instruction.
  /// @param rvalue the rvalue
  explicit RvalueInstr(std::shared_ptr<Rvalue> rvalue) : rvalue(std::move(rvalue)) {}

  void accept(common::SIRVisitor &v) override;

  /// @return the rvalue
  std::shared_ptr<Rvalue> getRvalue() const { return rvalue; }

  std::string textRepresentation() const override;
};

} // namespace ir
} // namespace seq
