#pragma once

#include <memory>
#include <string>

#include "base.h"
#include "types/types.h"

namespace seq {
namespace ir {

class Var;

class Lvalue : public AttributeHolder<Lvalue> {
private:
  std::shared_ptr<types::Type> type;

public:
  explicit Lvalue(std::shared_ptr<types::Type> type) : type(std::move(type)){};

  std::shared_ptr<types::Type> getType() { return type; }

  std::string referenceString() const override { return "lvalue"; };
};

class VarLvalue : public Lvalue {
private:
  std::weak_ptr<Var> var;

public:
  explicit VarLvalue(std::weak_ptr<Var> var);

  std::weak_ptr<Var> getVar() { return var; }

  std::string textRepresentation() const override;
};

class VarMemberLvalue : public Lvalue {
private:
  std::weak_ptr<Var> var;
  std::string field;

public:
  explicit VarMemberLvalue(std::weak_ptr<Var> var, std::string field);

  std::weak_ptr<Var> getVar() { return var; }
  std::string getField() const { return field; }

  std::string textRepresentation() const override;
};

} // namespace ir
} // namespace seq
