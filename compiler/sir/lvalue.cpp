#include "util/fmt/format.h"

#include "lvalue.h"
#include "var.h"

namespace seq {
namespace ir {

VarLvalue::VarLvalue(std::weak_ptr<Var> var)
    : Lvalue(var.lock()->getType()), var(var) {}

std::string VarLvalue::textRepresentation() const {
  return var.lock()->referenceString();
}

VarMemberLvalue::VarMemberLvalue(std::weak_ptr<Var> var, std::string field)
    : Lvalue(var.lock()->getType()), var(var), field(std::move(field)) {}

std::string VarMemberLvalue::textRepresentation() const {
  return fmt::format(FMT_STRING("{}.{}"), var.lock()->referenceString(), field);
}

} // namespace ir
} // namespace seq
