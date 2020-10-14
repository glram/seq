#pragma once

#include <memory>
#include <string>

#include "base.h"

#include "types/types.h"

namespace seq {
namespace ir {

namespace common {
class IRVisitor;
}

class Var;

class Lvalue : public AttributeHolder<Lvalue> {
public:
  virtual ~Lvalue() = default;

  virtual void accept(common::IRVisitor &v);

  virtual std::shared_ptr<types::Type> getType() = 0;
  std::string referenceString() const override { return "lvalue"; };
};

class VarLvalue : public Lvalue {
private:
  std::shared_ptr<Var> var;

public:
  explicit VarLvalue(std::shared_ptr<Var> var);

  void accept(common::IRVisitor &v) override;

  std::shared_ptr<types::Type> getType() override;

  std::shared_ptr<Var> getVar() { return var; }

  std::string textRepresentation() const override;
};

class VarMemberLvalue : public Lvalue {
private:
  std::shared_ptr<Var> var;
  std::string field;

public:
  VarMemberLvalue(std::shared_ptr<Var> var, std::string field);

  void accept(common::IRVisitor &v) override;

  std::shared_ptr<types::Type> getType() override;

  std::shared_ptr<Var> getVar() { return var; }
  std::string getField() const { return field; }

  std::string textRepresentation() const override;
};

} // namespace ir
} // namespace seq
