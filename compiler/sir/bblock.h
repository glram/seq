#pragma once

#include <memory>
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
  BasicBlock();
  BasicBlock(const BasicBlock &other);

  void add(std::shared_ptr<Instr> instruction);
  std::vector<std::shared_ptr<Instr>> getInstructions() const;

  void setTerminator(std::shared_ptr<Terminator> terminator);
  std::shared_ptr<Terminator> getTerminator() const;

  int getId();

  virtual std::string referenceString() const override;
  std::string textRepresentation() const override;
};
} // namespace ir
} // namespace seq