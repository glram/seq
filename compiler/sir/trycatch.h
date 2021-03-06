#pragma once

#include "base.h"
#include "restypes/types.h"
#include <vector>

namespace seq {
namespace ir {

class BasicBlock;

class TryCatch : public AttributeHolder {
private:
  static int currentId;

  std::vector<std::shared_ptr<TryCatch>> children;
  std::vector<std::shared_ptr<restypes::Type>> catchTypes;
  std::vector<std::weak_ptr<BasicBlock>> catchBlocks;
  std::weak_ptr<BasicBlock> finallyBlock;
  std::weak_ptr<TryCatch> parent;
  int id;

public:
  TryCatch();
  TryCatch(TryCatch &other);

  std::vector<std::shared_ptr<TryCatch>> getChildren() const;
  void addChild(std::shared_ptr<TryCatch> child);

  std::vector<std::shared_ptr<restypes::Type>> getCatchTypes() const;
  std::vector<std::weak_ptr<BasicBlock>> getCatchBlocks() const;
  void addCatch(std::shared_ptr<restypes::Type> catchType,
                std::weak_ptr<BasicBlock> handler);

  std::weak_ptr<BasicBlock> getFinallyBlock() const;
  void setFinallyBlock(std::weak_ptr<BasicBlock> finally);

  std::weak_ptr<TryCatch> getParent() const;
  int getId();

  std::string referenceString() const override;
  std::string textRepresentation() const override;
};
} // namespace ir
} // namespace seq
