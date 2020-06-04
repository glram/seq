#pragma once

#include "base.h"
#include <memory>

namespace seq {
namespace ir {

class Lvalue;
class Rvalue;

class Instr : public AttributeHolder<Instr> {};

class AssignInstr : public Instr {
private:
  std::shared_ptr<Lvalue> left;
  std::shared_ptr<Rvalue> right;
};

class RvalueInstr : public Instr {
  std::shared_ptr<Rvalue> right;
};

} // namespace ir
} // namespace seq