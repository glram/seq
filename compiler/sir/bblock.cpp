#include <algorithm>
#include <iterator>
#include <sstream>

#include "bblock.h"

using namespace seq;
using namespace ir;

BasicBlock::BasicBlock(int id) : statements{}, terminator{nullptr}, scope{}, id{id} { }

BasicBlock::BasicBlock(const BasicBlock &other) : statements{}, terminator{other.terminator}, scope{other.scope}, id{other.id}{
    std::copy(other.statements.begin(), other.statements.end(), std::back_inserter(statements));
}

void BasicBlock::add(std::shared_ptr<Statement> statement) {
    statements.push_back(statement);
}

std::vector<std::shared_ptr<Statement>> BasicBlock::getStatements() const {
    return statements;
}

void BasicBlock::setTerminator(std::shared_ptr<Terminator>terminator) {
    this->terminator = terminator;
}

std::shared_ptr<Terminator> BasicBlock::getTerminator() const {
    return terminator;
}

void BasicBlock::setScope(std::weak_ptr<Scope> scope) {
    this->scope = scope;
}

std::weak_ptr<Scope> BasicBlock::getScope() const {
    return scope;
}

std::string BasicBlock::textRepresentation() const {
    std::stringstream stream;

    stream << AttributeHolder::textRepresentation();
    stream <<  "bb" << id << " {";
    for (auto stmtPtr : statements) {
        stream << stmtPtr->textRepresentation() << "\n";
    }
    stream << terminator->textRepresentation() << "\n";
    stream << "}";

    return stream.str();
}

int BasicBlock::getId() const {
    return id;
}

void BasicBlock::setId(int id) {
    this->id = id;
}
