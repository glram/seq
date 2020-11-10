#include "instr.h"

#include "util/fmt/format.h"

#include "lvalue.h"
#include "rvalue.h"

#include "common/visitor.h"

namespace seq {
namespace ir {

void Instr::accept(common::SIRVisitor &v) { v.visit(getShared()); }

void AssignInstr::accept(common::SIRVisitor &v) { v.visit(getShared<AssignInstr>()); }

std::string AssignInstr::textRepresentation() const {
  return fmt::format(FMT_STRING("{} = {}; {}"), left->textRepresentation(),
                     right->textRepresentation(), attributeString());
}

void RvalueInstr::accept(common::SIRVisitor &v) { v.visit(getShared<RvalueInstr>()); }

std::string RvalueInstr::textRepresentation() const {
  return fmt::format("{}; {}", rvalue->textRepresentation(), attributeString());
}

} // namespace ir
} // namespace seq
