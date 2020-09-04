#pragma once

#include <memory>
#include <string>

#include "base.h"

namespace seq {
namespace ir {

class Lvalue;
class Rvalue;

class Instr : public AttributeHolder<Instr> {
public:
  virtual ~Instr() = default;
  std::string referenceString() const override { return "instr"; };
};

class AssignInstr : public Instr {
private:
  std::shared_ptr<Lvalue> left;
  std::shared_ptr<Rvalue> right;

public:
  explicit AssignInstr(std::shared_ptr<Lvalue> left,
                       std::shared_ptr<Rvalue> right)
      : left(std::move(left)), right(std::move(right)) {}

  std::shared_ptr<Rvalue> getRhs() const { return right; }
  std::shared_ptr<Lvalue> getLhs() const { return left; }

  std::string textRepresentation() const override;
};

class RvalueInstr : public Instr {
  std::shared_ptr<Rvalue> rvalue;

public:
  explicit RvalueInstr(std::shared_ptr<Rvalue> rvalue)
      : rvalue(std::move(rvalue)) {}

  std::shared_ptr<Rvalue> getRvalue() const { return rvalue; }

  std::string textRepresentation() const override;
};

} // namespace ir
} // namespace seq
