#pragma once

#include <memory>

#include "base.h"
#include "types/types.h"

namespace seq {
namespace ir {

namespace common {
class SIRVisitor;
}

class Var;

/// SIR object representing an argument to other SIR objects.
class Operand : public AttributeHolder<Operand> {
private:
  virtual std::shared_ptr<types::Type> opType() = 0;

public:
  virtual ~Operand() = default;

  virtual void accept(common::SIRVisitor &v);

  /// @tparam the expected type of SIR "type"
  /// @return the type of the operand
  template <typename Type = types::Type> std::shared_ptr<Type> getType() {
    return std::static_pointer_cast<Type>(opType());
  }

  std::string referenceString() const override { return "operand"; };
};

/// Operand representing the value of a variable.
class VarOperand : public Operand {
private:
  /// the variable
  std::shared_ptr<Var> var;

  std::shared_ptr<types::Type> opType() override;

public:
  /// Constructs a variable operand.
  /// @param var the variable
  explicit VarOperand(std::shared_ptr<Var> var) : var(std::move(var)) {}

  void accept(common::SIRVisitor &v) override;

  /// @return the variable
  std::shared_ptr<Var> getVar() { return var; }

  std::string textRepresentation() const override;
};

/// Operand representing the address of a variable.
class VarPointerOperand : public Operand {
private:
  /// the variable
  std::shared_ptr<Var> var;
  std::shared_ptr<types::PointerType> type;

  std::shared_ptr<types::Type> opType() override { return type; }

public:
  explicit VarPointerOperand(std::shared_ptr<types::PointerType> type,
                             std::shared_ptr<Var> var)
      : var(std::move(var)), type(std::move(type)) {}

  void accept(common::SIRVisitor &v) override;

  /// @return the variable
  std::shared_ptr<Var> getVar() { return var; }

  std::string textRepresentation() const override;
};

enum LiteralType { NONE, INT, UINT, FLOAT, BOOL, STR, SEQ };

/// Operand representing a literal value.
class LiteralOperand : public Operand {
private:
  LiteralType literalType;
  seq_int_t ival;
  double fval;
  bool bval;
  std::string sval;

  std::shared_ptr<types::Type> opType() override {
    switch (literalType) {
    case INT:
      return types::kIntType;
    case FLOAT:
      return types::kFloatType;
    case BOOL:
      return types::kBoolType;
    case STR:
      return types::kStringType;
    case SEQ:
      return types::kSeqType;
    default:
      return nullptr;
    }
  }

public:
  LiteralOperand() : literalType(LiteralType::NONE), ival(0), fval(0.0), bval(false) {}
  explicit LiteralOperand(seq_int_t ival)
      : literalType(LiteralType::INT), ival(ival), fval(0.0), bval(false) {}
  explicit LiteralOperand(double fval)
      : literalType(LiteralType::FLOAT), ival(0), fval(fval), bval(false) {}
  explicit LiteralOperand(bool bval)
      : literalType(LiteralType::BOOL), ival(0), fval(0.0), bval(bval) {}
  explicit LiteralOperand(std::string sval, bool seq = false)
      : literalType(seq ? LiteralType::SEQ : LiteralType::STR), ival(0), fval(0.0),
        bval(false), sval(std::move(sval)) {}

  void accept(common::SIRVisitor &v) override;

  /// @return the type of the literal
  LiteralType getLiteralType() const { return literalType; }

  /// @return the int value
  seq_int_t getIval() const { return ival; }

  /// @return the float value
  double getFval() const { return fval; }

  /// @return the bool value
  bool getBval() const { return bval; }

  /// @return the seq/str value
  std::string getSval() const { return sval; }

  std::string textRepresentation() const override;
};

} // namespace ir
} // namespace seq
