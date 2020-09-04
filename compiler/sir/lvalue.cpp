#include "util/fmt/format.h"

#include "lvalue.h"
#include "var.h"

namespace seq {
namespace ir {

VarLvalue::VarLvalue(std::weak_ptr<Var> var) : var(var) {}

std::shared_ptr<types::Type> VarLvalue::getType() {
  return var.lock()->getType();
}

std::string VarLvalue::textRepresentation() const {
  return var.lock()->referenceString();
}

VarMemberLvalue::VarMemberLvalue(std::weak_ptr<Var> var, std::string field)
    : var(var), field(std::move(field)) {}

std::shared_ptr<types::Type> VarMemberLvalue::getType() {
  return std::static_pointer_cast<types::RecordType>(var.lock()->getType())->getMemberType(field);
}

std::string VarMemberLvalue::textRepresentation() const {
  return fmt::format(FMT_STRING("{}.{}"), var.lock()->referenceString(), field);
}

} // namespace ir
} // namespace seq
