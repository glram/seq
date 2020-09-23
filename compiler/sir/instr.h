#pragma once

#include <memory>
#include <string>

#include "base.h"

#include "codegen/codegen.h"

namespace seq {
namespace ir {

class Lvalue;
class Rvalue;

class Instr : public AttributeHolder<Instr> {
public:
  virtual ~Instr() = default;

  virtual void accept(codegen::CodegenVisitor &v) { v.visit(getShared()); }
  std::string referenceString() const override { return "instr"; };
};

class AssignInstr : public Instr {
private:
  std::shared_ptr<Lvalue> left;
  std::shared_ptr<Rvalue> right;

public:
  explicit AssignInstr(std::shared_ptr<Lvalue> left, std::shared_ptr<Rvalue> right)
      : left(std::move(left)), right(std::move(right)) {}

  void accept(codegen::CodegenVisitor &v) override {
    v.visit(std::static_pointer_cast<AssignInstr>(getShared()));
  }

  std::shared_ptr<Rvalue> getRhs() const { return right; }
  std::shared_ptr<Lvalue> getLhs() const { return left; }

  std::string textRepresentation() const override;
};

class RvalueInstr : public Instr {
  std::shared_ptr<Rvalue> rvalue;

public:
  explicit RvalueInstr(std::shared_ptr<Rvalue> rvalue) : rvalue(std::move(rvalue)) {}

  void accept(codegen::CodegenVisitor &v) override {
    v.visit(std::static_pointer_cast<RvalueInstr>(getShared()));
  }

  std::shared_ptr<Rvalue> getRvalue() const { return rvalue; }

  std::string textRepresentation() const override;
};

} // namespace ir
} // namespace seq
