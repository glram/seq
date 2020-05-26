#include <algorithm>
#include <iterator>
#include <sstream>

#include "bblock.h"
#include "trycatch.h"

using namespace seq;
using namespace ir;

TryCatch::TryCatch()
    : children{}, catchTypes{}, catchBlocks{}, finallyBlock{}, parent{} {}

TryCatch::TryCatch(TryCatch &other)
    : children{}, catchTypes{}, catchBlocks{},
      finallyBlock{other.finallyBlock}, parent{other.parent} {
  std::copy(other.children.begin(), other.children.end(),
            std::back_inserter(children));
  std::copy(other.catchTypes.begin(), other.catchTypes.end(),
            std::back_inserter(catchTypes));
  std::copy(other.catchBlocks.begin(), other.catchBlocks.end(),
            std::back_inserter(catchBlocks));
}

std::vector<std::shared_ptr<TryCatch>> TryCatch::getChildren() const {
  return children;
}

void TryCatch::addChild(std::shared_ptr<TryCatch> child) {
  children.push_back(child);
}

std::vector<std::shared_ptr<restypes::Type>> TryCatch::getCatchTypes() const {
  return catchTypes;
}

std::vector<std::weak_ptr<BasicBlock>> TryCatch::getCatchBlocks() const {
  return catchBlocks;
}

void TryCatch::addCatch(std::shared_ptr<restypes::Type> catchType,
                        std::weak_ptr<BasicBlock> handler) {
  catchTypes.push_back(catchType);
  catchBlocks.push_back(handler);
}

std::weak_ptr<BasicBlock> TryCatch::getFinallyBlock() const {
  return finallyBlock;
}

void TryCatch::setFinallyBlock(std::weak_ptr<BasicBlock> finally) {
  finallyBlock = finally;
}

std::weak_ptr<TryCatch> TryCatch::getParent() const { return parent; }

std::string TryCatch::textRepresentation() const {
  std::stringstream stream;
  stream << AttributeHolder::textRepresentation() << "try [";
  for (auto it = children.begin(); it != children.end(); it++) {
    stream << (*it)->textRepresentation();
    if (it + 1 != children.end())
      stream << ",\n";
  }
  stream << "]{";
  // TODO better rendering of basic blocks
  for (int i = 0; i < catchBlocks.size(); i++) {
    auto handler = catchBlocks[i].lock();
    stream << catchTypes[i]->getName() << ": bb" << handler->getId();
    if (i + 1 != catchBlocks.size()) {
      stream << ", ";
    }
  }
  stream << "}";

  auto finally = finallyBlock.lock();
  if (finally)
    stream << " finally bb" << finally->getId();

  return stream.str();
}
