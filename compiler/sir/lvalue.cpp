#include "lvalue.h"
#include "var.h"

using namespace seq;
using namespace ir;

VarLvalue::VarLvalue(std::weak_ptr<Var> var)
    : Lvalue{var.lock()->getType()}, var{var} {}

std::weak_ptr<Var> VarLvalue::getVar() const { return var; }

std::string VarLvalue::textRepresentation() const {
  return var.lock()->referenceString();
}

VarMemberLvalue::VarMemberLvalue(std::weak_ptr<Var> var, std::string field)
    : Lvalue{var.lock()->getType()}, var{var}, field{field} {}

std::weak_ptr<Var> VarMemberLvalue::getVar() const { return var; }

std::string VarMemberLvalue::getField() const { return field; }

std::string VarMemberLvalue::textRepresentation() const {
  return var.lock()->referenceString() + "." + field;
}
