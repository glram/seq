#pragma once

#include <memory>

#include "base.h"

namespace seq {
namespace ir {

class Var;

class Operand : public AttributeHolder<Operand> {};

class VarOperand : public Operand {
private:
  std::weak_ptr<Var> var;
};

class VarMemberOperand : public Operand {
private:
  std::weak_ptr<Var> var;
  std::string field;
};

class LiteralOperand : public Operand {
  enum Type { NONE, INT, FLOAT, BOOL, STR, SEQ };
  seq_int_t ival;
  double fval;
  bool bval;
  std::string sval;
};

} // namespace ir
} // namespace seq