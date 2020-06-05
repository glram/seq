#include "operand.h"
#include "var.h"

using namespace seq;
using namespace ir;

VarOperand::VarOperand(std::weak_ptr<Var> var)
    : Operand{var.lock()->getType()}, var{var} {}

std::weak_ptr<Var> VarOperand::getVar() const { return var; }

std::string VarOperand::textRepresentation() const {
  return "read(" + var.lock()->referenceString() + ")";
};

VarMemberOperand::VarMemberOperand(std::weak_ptr<Var> var, std::string field)
    : Operand{var.lock()->getType()->getMemberType(field).lock()}, var{var},
      field{field} {}

std::weak_ptr<Var> VarMemberOperand::getVar() const { return var; }

std::string VarMemberOperand::getField() const { return field; }

std::string VarMemberOperand::textRepresentation() const {
  return "read(" + var.lock()->referenceString() + "." + field + ")";
}

LiteralType LiteralOperand::getLiteralType() const { return literalType; }

seq_int_t LiteralOperand::getIval() { return ival; }

double LiteralOperand::getFval() { return fval; }

bool LiteralOperand::getBval() { return bval; }

std::string LiteralOperand::getSval() { return sval; }

std::string LiteralOperand::textRepresentation() const {
  switch (literalType) {
  case LiteralType::FLOAT:
    return std::to_string(fval);
  case LiteralType::BOOL:
    return bval ? "true" : "false";
  case LiteralType::INT:
    return std::to_string(ival);
  case LiteralType::NONE:
    return "None";
  case LiteralType::SEQ:
    return "s'" + sval + "'";
  case LiteralType::STR:
    return "'" + sval + "'";
  }
}
