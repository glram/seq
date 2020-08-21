#include "util/fmt/format.h"

#include "var.h"

namespace seq {
namespace ir {

std::string Var::textRepresentation() const {
  return fmt::format(FMT_STRING("{} {}; {}"), type->getName(),
                     referenceString(), attributeString());
}

std::string Var::referenceString() const {
  return fmt::format(FMT_STRING("${}#{}"), name, id);
}

} // namespace ir
} // namespace seq
