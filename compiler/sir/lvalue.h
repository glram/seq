#pragma once

#include <memory>
#include <string>

#include "base.h"
#include "types/types.h"

#include "codegen/codegen.h"

namespace seq {
namespace ir {

class Var;

class Lvalue : public AttributeHolder<Lvalue> {
public:
  virtual ~Lvalue() = default;

  virtual void accept(codegen::CodegenVisitor &v) { v.visit(getShared()); }

  virtual std::shared_ptr<types::Type> getType() = 0;
  std::string referenceString() const override { return "lvalue"; };
};

class VarLvalue : public Lvalue {
private:
  std::weak_ptr<Var> var;

public:
  explicit VarLvalue(std::weak_ptr<Var> var);

  void accept(codegen::CodegenVisitor &v) override {
    v.visit(std::static_pointer_cast<VarLvalue>(getShared()));
  }

  std::shared_ptr<types::Type> getType() override;

  std::weak_ptr<Var> getVar() { return var; }

  std::string textRepresentation() const override;
};

class VarMemberLvalue : public Lvalue {
private:
  std::weak_ptr<Var> var;
  std::string field;

public:
  VarMemberLvalue(std::weak_ptr<Var> var, std::string field);

  void accept(codegen::CodegenVisitor &v) override {
    v.visit(std::static_pointer_cast<VarMemberLvalue>(getShared()));
  }

  std::shared_ptr<types::Type> getType() override;

  std::weak_ptr<Var> getVar() { return var; }
  std::string getField() const { return field; }

  std::string textRepresentation() const override;
};

} // namespace ir
} // namespace seq
