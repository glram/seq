#pragma once

#include <memory>

#include "base.h"
#include "types/types.h"

namespace seq {
namespace ir {

class Var;

class Operand : public AttributeHolder<Operand> {
private:
  std::shared_ptr<types::Type> type;

public:
  Operand(std::shared_ptr<types::Type> type) : type{type} {};

  std::shared_ptr<types::Type> getType() const { return type; };
  std::string referenceString() const override { return "operand"; };
};

class VarOperand : public Operand {
private:
  std::weak_ptr<Var> var;

public:
  explicit VarOperand(std::weak_ptr<Var> var);

  std::weak_ptr<Var> getVar() { return var; }

  std::string textRepresentation() const override;
};

class VarMemberOperand : public Operand {
private:
  std::weak_ptr<Var> var;
  std::string field;

public:
  explicit VarMemberOperand(std::weak_ptr<Var> var, std::string field);

  std::weak_ptr<Var> getVar() { return var; }
  std::string getField() const { return field; }

  std::string textRepresentation() const override;
};

enum LiteralType { NONE, INT, FLOAT, BOOL, STR, SEQ };

class LiteralOperand : public Operand {
private:
  LiteralType literalType;
  seq_int_t ival;
  double fval;
  bool bval;
  std::string sval;
  bool isSeq;

public:
  LiteralOperand()
      : Operand(nullptr), literalType(LiteralType::NONE), ival(0), fval(0.0),
        bval(false), sval(), isSeq(false) {}
  explicit LiteralOperand(seq_int_t ival)
      : Operand(types::kIntType), literalType(LiteralType::INT), ival(ival),
        fval(0.0), bval(false), sval(), isSeq(false) {}
  explicit LiteralOperand(double fval)
      : Operand(types::kFloatType), literalType(LiteralType::FLOAT), ival(0),
        fval(fval), bval(false), sval(), isSeq(false) {}
  explicit LiteralOperand(bool bval)
      : Operand(types::kBoolType), literalType(LiteralType::BOOL), ival(0),
        fval(0.0), bval(bval), sval(), isSeq(false) {}
  explicit LiteralOperand(std::string sval, bool seq = false)
      : Operand(seq ? types::kSeqType : types::kStringType),
        literalType(seq ? LiteralType::SEQ : LiteralType::STR), ival(0),
        fval(0.0), bval(false), sval(std::move(sval)), isSeq(seq) {}

  LiteralType getLiteralType() const { return literalType; }
  seq_int_t getIval() const { return ival; }
  double getFval() const { return fval; }
  bool getBval() const { return bval; }
  std::string getSval() const { return sval; }

  std::string textRepresentation() const override;
};

} // namespace ir
} // namespace seq
