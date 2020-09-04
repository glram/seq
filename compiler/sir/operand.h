#pragma once

#include <memory>

#include "base.h"
#include "types/types.h"

namespace seq {
namespace ir {

class Var;

class Operand : public AttributeHolder<Operand> {
public:
  virtual ~Operand() = default;

  virtual std::shared_ptr<types::Type> getType() = 0;
  std::string referenceString() const override { return "operand"; };
};

class VarOperand : public Operand {
private:
  std::weak_ptr<Var> var;

public:
  explicit VarOperand(std::weak_ptr<Var> var);
  std::shared_ptr<types::Type> getType() override;

  std::weak_ptr<Var> getVar() { return var; }

  std::string textRepresentation() const override;
};

enum LiteralType { NONE, INT, UINT, FLOAT, BOOL, STR, SEQ };

class LiteralOperand : public Operand {
private:
  LiteralType literalType;
  seq_int_t ival;
  uint64_t uival;
  double fval;
  bool bval;
  std::string sval;
  bool isSeq;

public:
  LiteralOperand()
      : literalType(LiteralType::NONE), ival(0), uival(0), fval(0.0), bval(false), sval(),
        isSeq(false) {}
  explicit LiteralOperand(seq_int_t ival)
      : literalType(LiteralType::INT), ival(ival), uival(0), fval(0.0), bval(false),
        sval(), isSeq(false) {}
  explicit LiteralOperand(uint64_t uival)
      : literalType(LiteralType::INT), ival(0), uival(uival), fval(0.0), bval(false),
        sval(), isSeq(false) {}
  explicit LiteralOperand(double fval)
      : literalType(LiteralType::FLOAT), ival(0), uival(0), fval(fval), bval(false),
        sval(), isSeq(false) {}
  explicit LiteralOperand(bool bval)
      : literalType(LiteralType::BOOL), ival(0), uival(0), fval(0.0), bval(bval), sval(),
        isSeq(false) {}
  explicit LiteralOperand(std::string sval, bool seq = false)
      : literalType(seq ? LiteralType::SEQ : LiteralType::STR), ival(0),
        uival(0), fval(0.0), bval(false), sval(std::move(sval)), isSeq(seq) {}

  std::shared_ptr<types::Type> getType() override {
    switch (literalType) {
    case INT:
      return types::kIntType;
    case UINT:
      return types::kUIntType;
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

  LiteralType getLiteralType() const { return literalType; }
  seq_int_t getIval() const { return ival; }
  uint64_t getUIval() const { return uival; }
  double getFval() const { return fval; }
  bool getBval() const { return bval; }
  std::string getSval() const { return sval; }

  std::string textRepresentation() const override;
};

} // namespace ir
} // namespace seq
