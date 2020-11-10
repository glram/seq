#pragma once

#include <memory>
#include <string>

#include "base.h"

#include "types/types.h"

namespace seq {
namespace ir {

namespace common {
class SIRVisitor;
}

class Var;

/// SIR object representing the left-hand side of an assignment (a memory location).
class Lvalue : public AttributeHolder<Lvalue> {
private:
  virtual std::shared_ptr<types::Type> lvalType() = 0;

public:
  virtual ~Lvalue() = default;

  virtual void accept(common::SIRVisitor &v);

  /// @tparam the expected type of SIR "type"
  /// @return the type of the lvalue
  template <typename Type = types::Type> std::shared_ptr<Type> getType() {
    return std::static_pointer_cast<Type>(lvalType());
  }

  std::string referenceString() const override { return "lvalue"; };
};

/// Lvalue representing a variable.
class VarLvalue : public Lvalue {
private:
  /// the variable
  std::shared_ptr<Var> var;

  std::shared_ptr<types::Type> lvalType() override;

public:
  /// Constructs a var lvalue.
  /// @param var the variable
  explicit VarLvalue(std::shared_ptr<Var> var);

  void accept(common::SIRVisitor &v) override;

  /// @return the variable
  std::shared_ptr<Var> getVar() { return var; }

  std::string textRepresentation() const override;
};

/// Lvalue representing a particular field of a variable.
class VarMemberLvalue : public Lvalue {
private:
  /// the variable
  std::shared_ptr<Var> var;
  /// the field
  std::string field;

  std::shared_ptr<types::Type> lvalType() override;

public:
  /// Constructs a var member lvalue
  VarMemberLvalue(std::shared_ptr<Var> var, std::string field);

  void accept(common::SIRVisitor &v) override;

  /// @return the variable
  std::shared_ptr<Var> getVar() { return var; }

  /// @return the field
  std::string getField() const { return field; }

  std::string textRepresentation() const override;
};

} // namespace ir
} // namespace seq
