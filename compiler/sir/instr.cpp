#include "instr.h"
#include "lvalue.h"
#include "rvalue.h"

namespace seq {
namespace ir {

std::string AssignInstr::textRepresentation() const {
  return fmt::format(FMT_STRING("{} = {}; {}"), left->textRepresentation(),
                     right->textRepresentation(), attributeString());
}

std::string RvalueInstr::textRepresentation() const {
  return fmt::format("{}; {}", rvalue->textRepresentation(), attributeString());
}

} // namespace ir
} // namespace seq
