#pragma once

#include "base.h"
#include "types/types.h"
#include <utility>
#include <vector>

namespace seq {
namespace ir {

class BasicBlock;
class Var;

class TryCatch : public AttributeHolder<TryCatch> {
private:
  static int currentId;

  std::vector<std::shared_ptr<TryCatch>> children;
  std::vector<std::shared_ptr<types::Type>> catchTypes;
  std::vector<std::string> catchVarNames;
  std::vector<std::shared_ptr<Var>> catchVars;
  std::vector<std::weak_ptr<BasicBlock>> catchBlocks;
  std::weak_ptr<BasicBlock> finallyBlock;
  std::weak_ptr<TryCatch> parent;
  int id;

public:
  TryCatch() : id(currentId++){};

  std::vector<std::shared_ptr<TryCatch>> getChildren() { return children; }
  void addChild(std::shared_ptr<TryCatch> child) {
    children.push_back(child);
    child->parent = getShared();
  }

  std::vector<std::shared_ptr<types::Type>> getCatchTypes() {
    return catchTypes;
  }
  std::vector<std::weak_ptr<BasicBlock>> getCatchBlocks() {
    return catchBlocks;
  }
  void addCatch(std::shared_ptr<types::Type> catchType, std::string name,
                std::weak_ptr<BasicBlock> handler);

  std::weak_ptr<BasicBlock> getFinallyBlock() { return finallyBlock; }
  void setFinallyBlock(std::weak_ptr<BasicBlock> finally) {
    finallyBlock = std::move(finally);
  }

  std::weak_ptr<TryCatch> getParent() { return parent; }
  int getId() { return id; }

  std::shared_ptr<Var> getVar(int i) { return catchVars[i]; }

  std::string referenceString() const override;
  std::string textRepresentation() const override;
};

} // namespace ir
} // namespace seq
