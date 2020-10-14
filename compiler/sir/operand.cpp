#include "util/fmt/format.h"

#include "operand.h"
#include "var.h"

#include "common/visitor.h"

namespace seq {
namespace ir {

void Operand::accept(common::IRVisitor &v) { v.visit(getShared()); }

void VarOperand::accept(common::IRVisitor &v) {
  v.visit(std::static_pointer_cast<VarOperand>(getShared()));
}

std::shared_ptr<types::Type> VarOperand::getType() { return var->getType(); }

std::string VarOperand::textRepresentation() const { return var->referenceString(); }

void VarPointerOperand::accept(common::IRVisitor &v) {
  v.visit(std::static_pointer_cast<VarPointerOperand>(getShared()));
}

std::string VarPointerOperand::textRepresentation() const {
  return fmt::format(FMT_STRING("&{}"), var->referenceString());
}

void LiteralOperand::accept(common::IRVisitor &v) {
  v.visit(std::static_pointer_cast<LiteralOperand>(getShared()));
}

std::string LiteralOperand::textRepresentation() const {
  switch (literalType) {
  case LiteralType::FLOAT:
    return std::to_string(fval);
  case LiteralType::BOOL:
    return bval ? "true" : "false";
  case LiteralType::INT:
    return std::to_string(ival);
  case LiteralType::UINT:
    return std::to_string(ival) + "u";
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
