#pragma once

#include <memory>
#include <string>

#include "base.h"
#include "types/types.h"

namespace seq {
namespace ir {

class Var;

class Lvalue : public AttributeHolder<Lvalue> {
public:
  virtual ~Lvalue() = default;

  virtual std::shared_ptr<types::Type> getType() = 0;
  std::string referenceString() const override { return "lvalue"; };
};

class VarLvalue : public Lvalue {
private:
  std::weak_ptr<Var> var;

public:
  explicit VarLvalue(std::weak_ptr<Var> var);
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
  std::shared_ptr<types::Type> getType() override;

  std::weak_ptr<Var> getVar() { return var; }
  std::string getField() const { return field; }

  std::string textRepresentation() const override;
};

} // namespace ir
} // namespace seq
