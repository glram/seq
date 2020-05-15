#pragma once

#include "base.h"
#include "restypes/types.h"
#include <vector>

namespace seq {
namespace ir {

class BasicBlock;

class TryCatch : public AttributeHolder {
public:
  std::vector<TryCatch> children;
  std::vector<restypes::Type> catchTypes;
  std::vector<std::weak_ptr<BasicBlock>> catchBlocks;
  std::weak_ptr<BasicBlock> finallyBlock;
};
} // namespace ir
} // namespace seq
