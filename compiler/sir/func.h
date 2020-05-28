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

  std::weak_ptr<IRModule> module;

public:
  Func(std::string name, std::vector<std::string> argNames,
       std::shared_ptr<restypes::FuncType> type);
  Func(const Func &other);

  std::vector<std::shared_ptr<Var>> getArgVars();
  std::vector<std::string> getArgNames();
  std::shared_ptr<Var> getArgVar(std::string name);

  void addVar(std::shared_ptr<Var> var);

  std::vector<std::shared_ptr<BasicBlock>> getBlocks();
  void addBlock(std::shared_ptr<BasicBlock> block);

  std::weak_ptr<IRModule> getModule();
  void setModule(std::weak_ptr<IRModule> module);

  virtual std::string referenceString() const override;
  std::string textRepresentation() const override;
};
} // namespace ir
} // namespace seq