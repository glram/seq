#pragma once

#include <memory>
#include <string>
#include <vector>

#include "restypes/types.h"
#include "var.h"

namespace seq {
namespace ir {

class IRModule;
class BasicBlock;

class Func : public Var {
private:
  std::vector<std::shared_ptr<Var>> argVars;
  std::vector<std::string> argNames;

  std::vector<std::shared_ptr<Var>> vars;
  std::vector<std::shared_ptr<BasicBlock>> blocks;

  std::shared_ptr<restypes::Type> rType;

  std::weak_ptr<IRModule> module;

public:
  Func();
  Func(const Func &other);

  void setArgVars(std::vector<std::shared_ptr<Var>> argVars);
  std::vector<std::shared_ptr<Var>> getArgVars();

  void setArgNames(std::vector<std::string> argNames);
  std::vector<std::string> getArgNames();

  std::shared_ptr<Var> getArgVar(std::string name);

  std::vector<std::shared_ptr<BasicBlock>> getBlocks();
  void addBlock(std::shared_ptr<BasicBlock> block);

  void setRType(std::shared_ptr<restypes::Type> rType);
  std::shared_ptr<restypes::Type> getRType();

  std::weak_ptr<IRModule> getModule();

  std::string textRepresentation() const;
};
} // namespace ir
} // namespace seq