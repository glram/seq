#include <algorithm>
#include <iterator>
#include <sstream>

#include "bblock.h"
#include "instr.h"
#include "terminator.h"

using namespace seq;
using namespace ir;

BasicBlock::BasicBlock()
    : instructions{}, terminator{nullptr}, id{currentId++} {}

BasicBlock::BasicBlock(const BasicBlock &other)
    : instructions{}, terminator{other.terminator}, id{other.id} {
  std::copy(other.instructions.begin(), other.instructions.end(),
            std::back_inserter(instructions));
}

void BasicBlock::add(std::shared_ptr<Instr> instruction) {
  instructions.push_back(instruction);
}

std::vector<std::shared_ptr<Instr>> BasicBlock::getInstructions() const {
  return instructions;
}

void BasicBlock::setTerminator(std::shared_ptr<Terminator> terminator) {
  this->terminator = terminator;
}

std::shared_ptr<Terminator> BasicBlock::getTerminator() const {
  return terminator;
}

int BasicBlock::getId() { return id; }

std::string BasicBlock::referenceString() const {
  return "bb#" + std::to_string(id);
}

std::string BasicBlock::textRepresentation() const {
  std::stringstream stream;

  stream << referenceString() << " {";
  for (auto instrPtr : instructions) {
    stream << instrPtr->textRepresentation() << "\n";
  }
  stream << terminator->textRepresentation() << "\n";
  stream << "}; " << attributeString();

  return stream.str();
}