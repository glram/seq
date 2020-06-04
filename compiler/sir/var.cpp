#include <sstream>

#include "var.h"

using namespace seq;
using namespace ir;

Var::Var(std::string name, std::shared_ptr<types::Type> type)
    : name{name}, type{type}, id{varNum++} {}

Var::Var(std::shared_ptr<types::Type> type)
    : name{"unnamed"}, type{type}, id{varNum++} {}

void Var::setType(std::shared_ptr<types::Type> type) { this->type = type; }

std::string Var::getName() { return name; }

std::shared_ptr<types::Type> Var::getType() { return type; }

int Var::getId() { return id; }

std::string Var::textRepresentation() const {
  std::stringstream stream;
  stream << id << "$" << name << attributeString();
  return stream.str();
}

std::string Var::referenceString() const {
  return "$" + name + "#" + std::to_string(id);
}
