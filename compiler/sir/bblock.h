#pragma once

#include <memory>
#include <string>
#include <vector>

#include "base.h"

namespace seq {
namespace ir {

class Instr;
class Terminator;

class BasicBlock : public AttributeHolder<BasicBlock> {
private:
  static int currentId;

  std::vector<std::shared_ptr<Instr>> instructions;
  std::shared_ptr<Terminator> terminator;

  int id;

public:
  BasicBlock() : id(currentId++) {}

  void add(std::shared_ptr<Instr> instruction) {
    instructions.push_back(instruction);
  }
  std::vector<std::shared_ptr<Instr>> getInstructions() { return instructions; }

  void setTerminator(std::shared_ptr<Terminator> t) {
    terminator = std::move(t);
  }
  std::shared_ptr<Terminator> getTerminator() { return terminator; }

  int getId() const { return id; }

  std::string referenceString() const override;
  std::string textRepresentation() const override;
};

} // namespace ir
} // namespace seq
