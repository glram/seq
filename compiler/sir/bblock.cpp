#include <algorithm>
#include <iterator>
#include <sstream>

#include "bblock.h"
#include "stmt.h"
#include "terminator.h"

using namespace seq;
using namespace ir;

BasicBlock::BasicBlock() : statements{}, terminator{nullptr}, id{currentId++} {}

BasicBlock::BasicBlock(const BasicBlock &other)
    : statements{}, terminator{other.terminator}, id{other.id} {
  std::copy(other.statements.begin(), other.statements.end(),
            std::back_inserter(statements));
}

void BasicBlock::add(std::shared_ptr<Statement> statement) {
  statements.push_back(statement);
}

std::vector<std::shared_ptr<Statement>> BasicBlock::getStatements() const {
  return statements;
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

  stream << AttributeHolder::textRepresentation();
  stream << referenceString() << " {";
  for (auto stmtPtr : statements) {
    stream << stmtPtr->textRepresentation() << "\n";
  }
  stream << terminator->textRepresentation() << "\n";
  stream << "}";

  return stream.str();
}