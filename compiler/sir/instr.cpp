#include "instr.h"

#include "util/fmt/format.h"

#include "lvalue.h"
#include "rvalue.h"

#include "common/visitor.h"

namespace seq {
namespace ir {

void Instr::accept(common::IRVisitor &v) { v.visit(getShared()); }

void AssignInstr::accept(common::IRVisitor &v) {
  v.visit(std::static_pointer_cast<AssignInstr>(getShared()));
}

std::string AssignInstr::textRepresentation() const {
  return fmt::format(FMT_STRING("{} = {}; {}"), left->textRepresentation(),
                     right->textRepresentation(), attributeString());
}

void RvalueInstr::accept(common::IRVisitor &v) {
  v.visit(std::static_pointer_cast<RvalueInstr>(getShared()));
}

std::string RvalueInstr::textRepresentation() const {
  return fmt::format("{}; {}", rvalue->textRepresentation(), attributeString());
}

} // namespace ir
} // namespace seq
