#include "util/fmt/format.h"

#include "var.h"

namespace seq {
namespace ir {

int Var::currentId = 0;

void Var::resetId() { currentId = 0; }

std::string Var::textRepresentation() const {
  return fmt::format(FMT_STRING("{}: {}; {};"), referenceString(),
                     type->referenceString(), attributeString());
}

std::string Var::referenceString() const {
  return fmt::format(FMT_STRING("${}#{}"), name, id);
}

} // namespace ir
} // namespace seq
