#include "util/fmt/format.h"

#include "lvalue.h"
#include "var.h"

#include "common/visitor.h"

namespace seq {
namespace ir {

void Lvalue::accept(common::IRVisitor &v) { v.visit(getShared()); }

VarLvalue::VarLvalue(std::shared_ptr<Var> var) : var(std::move(var)) {}

void VarLvalue::accept(common::IRVisitor &v) {
  v.visit(std::static_pointer_cast<VarLvalue>(getShared()));
}

std::shared_ptr<types::Type> VarLvalue::getType() { return var->getType(); }

std::string VarLvalue::textRepresentation() const { return var->referenceString(); }

VarMemberLvalue::VarMemberLvalue(std::shared_ptr<Var> var, std::string field)
    : var(std::move(var)), field(std::move(field)) {}

void VarMemberLvalue::accept(common::IRVisitor &v) {
  v.visit(std::static_pointer_cast<VarMemberLvalue>(getShared()));
}

std::shared_ptr<types::Type> VarMemberLvalue::getType() {
  return std::static_pointer_cast<types::RecordType>(var->getType())
      ->getMemberType(field);
}

std::string VarMemberLvalue::textRepresentation() const {
  return fmt::format(FMT_STRING("{}.{}"), var->referenceString(), field);
}

} // namespace ir
} // namespace seq
