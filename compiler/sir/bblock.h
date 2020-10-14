#pragma once

#include <memory>
#include <string>
#include <vector>

#include "base.h"

namespace seq {
namespace ir {

namespace common {
class IRVisitor;
}

class Instr;
class Terminator;
class TryCatch;

class BasicBlock : public AttributeHolder<BasicBlock> {
private:
  static int currentId;

  std::vector<std::shared_ptr<Instr>> instructions;
  std::shared_ptr<Terminator> terminator;

  int id;

  std::shared_ptr<TryCatch> tc;

public:
  explicit BasicBlock(std::shared_ptr<TryCatch> tc = nullptr)
      : id(currentId++), tc(std::move(tc)) {}

  static void resetId();

  void add(std::shared_ptr<Instr> instruction) { instructions.push_back(instruction); }
  std::vector<std::shared_ptr<Instr>> getInstructions() { return instructions; }

  void setTerminator(std::shared_ptr<Terminator> t) { terminator = std::move(t); }
  std::shared_ptr<Terminator> getTerminator() { return terminator; }

  int getId() const { return id; }

  void setTryCatch(std::shared_ptr<TryCatch> newTc) { tc = std::move(newTc); }
  std::shared_ptr<TryCatch> getTryCatch() { return tc; }

  std::string referenceString() const override;
  std::string textRepresentation() const override;

  void accept(common::IRVisitor &v);
};

} // namespace ir
} // namespace seq
