#include "util/fmt/format.h"

#include "operand.h"
#include "var.h"

namespace seq {
namespace ir {

VarOperand::VarOperand(std::weak_ptr<Var> var) : var(var) {}

std::shared_ptr<types::Type> VarOperand::getType() {
  return var.lock()->getType();
}

std::string VarOperand::textRepresentation() const {
  return var.lock()->referenceString();
};

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
    return fmt::format(FMT_STRING("s'{}'"), sval);
  case LiteralType::STR:
    return fmt::format(FMT_STRING("'{}'"), sval);
  }
}

} // namespace ir
} // namespace seq