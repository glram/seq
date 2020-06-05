#pragma once

#include "base.h"
#include <memory>

namespace seq {
namespace ir {

class Lvalue;
class Rvalue;

class Instr : public AttributeHolder<Instr> {
public:
  std::string referenceString() const override { return "instr"; };
};

class AssignInstr : public Instr {
private:
  std::shared_ptr<Lvalue> left;
  std::shared_ptr<Rvalue> right;

public:
  explicit AssignInstr(std::shared_ptr<Lvalue> left,
                       std::shared_ptr<Rvalue> right);

  std::shared_ptr<Rvalue> getRhs() const;
  std::shared_ptr<Lvalue> getLhs() const;

  std::string textRepresentation() const override;
};

class RvalueInstr : public Instr {
  std::shared_ptr<Rvalue> rvalue;

public:
  explicit RvalueInstr(std::shared_ptr<Rvalue> rvalue);

  std::shared_ptr<Rvalue> getRvalue() const;

  std::string textRepresentation() const override;
};

} // namespace ir
} // namespace seq