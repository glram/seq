#include "var.h"

#include "util/fmt/format.h"

#include "common/visitor.h"

namespace seq {
namespace ir {

int Var::currentId = 0;

void Var::resetId() { currentId = 0; }

void Var::accept(common::SIRVisitor &v) { v.visit(getShared()); }

std::string Var::textRepresentation() const {
  return fmt::format(FMT_STRING("{}: {}; {}"), referenceString(),
                     type->referenceString(), attributeString());
}

std::string Var::referenceString() const {
  return fmt::format(FMT_STRING("{}.var_{}"), name, id);
}

} // namespace ir
} // namespace seq
