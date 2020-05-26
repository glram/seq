#include <algorithm>
#include <iterator>
#include <sstream>

#include "bblock.h"
#include "func.h"
#include "var.h"

using namespace seq;
using namespace ir;

Func::Func(std::string name, std::vector<std::string> argNames,
           std::shared_ptr<restypes::FuncType> type)
    : Var{name, std::dynamic_pointer_cast<restypes::Type>(type)},
      argNames{argNames}, vars{}, blocks{} {

  argVars = std::vector<std::shared_ptr<Var>>{};
  auto argTypes = type->getArgTypes();
  for (int i = 0; i < argTypes.size(); i++) {
    argVars.push_back(std::make_shared<Var>(argNames[i], argTypes[i]));
  }
}

Func::Func(const Func &other)
    : Var{other.name, other.type}, argVars{}, argNames{other.argNames}, vars{},
      blocks{}, module{other.module} {
  std::copy(other.argVars.begin(), other.argVars.end(),
            std::back_inserter(argVars));
  std::copy(other.vars.begin(), other.vars.end(), std::back_inserter(vars));
  std::copy(other.blocks.begin(), other.blocks.end(),
            std::back_inserter(blocks));
}

std::vector<std::shared_ptr<Var>> Func::getArgVars() { return argVars; }

std::vector<std::string> Func::getArgNames() { return argNames; }

std::shared_ptr<Var> Func::getArgVar(std::string name) {
  auto it = std::find(argNames.begin(), argNames.end(), name);
  return (it == argVars.end()) ? std::shared_ptr<Var>{}
                               : argVars[it - argNames.begin()];
}

std::vector<std::shared_ptr<BasicBlock>> Func::getBlocks() { return blocks; }

void Func::addBlock(std::shared_ptr<BasicBlock> block) {
  blocks.push_back(block);
  block->setId(blocks.size() - 1);
}

std::weak_ptr<IRModule> Func::getModule() { return module; }

std::string Func::textRepresentation() const {
  std::stringstream stream;
  stream << "func " << name << "(";
  for (int i = 0; i < argNames.size(); i++) {
    stream << argNames[i] << ": " << argVars[i]->textRepresentation();
    if (i + 1 != argNames.size())
      stream << ", ";
  }

  stream << ")[";
  for (auto it = vars.begin(); it != vars.end(); it++) {
    stream << (*it)->textRepresentation();
    if (it + 1 != vars.end())
      stream << ", ";
  }

  stream << "]{\n";
  for (const auto &block : blocks) {
    stream << block->textRepresentation() << "\n";
  }
  stream << "}";
  return stream.str();
}

void Func::addVar(std::shared_ptr<Var> var) { vars.push_back(var); }

void Func::setModule(std::weak_ptr<IRModule> module) { this->module = module; }
