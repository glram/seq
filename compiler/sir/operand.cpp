#include "util/fmt/format.h"

#include "operand.h"
#include "var.h"

#include "common/visitor.h"

namespace seq {
namespace ir {

void Operand::accept(common::SIRVisitor &v) { v.visit(getShared()); }

void VarOperand::accept(common::SIRVisitor &v) { v.visit(getShared<VarOperand>()); }

std::shared_ptr<types::Type> VarOperand::opType() { return var->getType(); }

std::string VarOperand::textRepresentation() const { return var->referenceString(); }

void VarPointerOperand::accept(common::SIRVisitor &v) {
  v.visit(getShared<VarPointerOperand>());
}

std::string VarPointerOperand::textRepresentation() const {
  return fmt::format(FMT_STRING("&{}"), var->referenceString());
}

void LiteralOperand::accept(common::SIRVisitor &v) {
  v.visit(getShared<LiteralOperand>());
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
