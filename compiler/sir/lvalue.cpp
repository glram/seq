#include "util/fmt/format.h"

#include "lvalue.h"
#include "var.h"

#include "common/visitor.h"

namespace seq {
namespace ir {

void Lvalue::accept(common::SIRVisitor &v) { v.visit(getShared()); }

VarLvalue::VarLvalue(std::shared_ptr<Var> var) : var(std::move(var)) {}

void VarLvalue::accept(common::SIRVisitor &v) { v.visit(getShared<VarLvalue>()); }

std::shared_ptr<types::Type> VarLvalue::lvalType() { return var->getType(); }

std::string VarLvalue::textRepresentation() const { return var->referenceString(); }

VarMemberLvalue::VarMemberLvalue(std::shared_ptr<Var> var, std::string field)
    : var(std::move(var)), field(std::move(field)) {}

void VarMemberLvalue::accept(common::SIRVisitor &v) {
  v.visit(getShared<VarMemberLvalue>());
}

std::shared_ptr<types::Type> VarMemberLvalue::lvalType() {
  return std::static_pointer_cast<types::MemberedType>(var->getType())
      ->getMemberType(field);
}

std::string VarMemberLvalue::textRepresentation() const {
  return fmt::format(FMT_STRING("{}.{}"), var->referenceString(), field);
}

} // namespace ir
} // namespace seq
