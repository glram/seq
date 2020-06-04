#include <algorithm>
#include <iterator>
#include <sstream>

#include "module.h"
#include "var.h"

using namespace seq;
using namespace ir;

IRModule::IRModule(std::string name) : name{name}, globals{} {}

IRModule::IRModule(const IRModule &other) : name{name}, globals{} {
  std::copy(other.globals.begin(), other.globals.end(),
            std::back_inserter(globals));
}

std::vector<std::shared_ptr<Var>> IRModule::getGlobals() { return globals; }

void IRModule::addGlobal(std::shared_ptr<Var> var) { globals.push_back(var); }

std::string IRModule::getName() { return name; }

std::string IRModule::textRepresentation() const {
  std::stringstream stream;

  stream << "module " << name << "{\n";
  for (const auto &global : globals) {
    stream << global->textRepresentation() << "\n";
  }
  stream << "}; " << attributeString();
  return stream.str();
}
