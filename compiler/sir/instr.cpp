#include <sstream>

#include "instr.h"
#include "lvalue.h"
#include "rvalue.h"

using namespace seq;
using namespace ir;

AssignInstr::AssignInstr(std::shared_ptr<Lvalue> left,
                         std::shared_ptr<Rvalue> right)
    : left{left}, right{right} {}

std::shared_ptr<Rvalue> AssignInstr::getRhs() const { return right; }

std::shared_ptr<Lvalue> AssignInstr::getLhs() const { return left; }

std::string AssignInstr::textRepresentation() const {
  std::stringstream stream;

  stream << left->textRepresentation() << " = " << right->textRepresentation()
         << "; " << attributeString();
  return stream.str();
}

RvalueInstr::RvalueInstr(std::shared_ptr<Rvalue> rvalue) : rvalue{rvalue} {}

std::shared_ptr<Rvalue> RvalueInstr::getRvalue() const { return rvalue; }

std::string RvalueInstr::textRepresentation() const {
  std::stringstream stream;

  stream << rvalue->textRepresentation() << "; " << attributeString();
  return stream.str();
}
