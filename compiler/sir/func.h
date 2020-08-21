#pragma once

#include <memory>
#include <string>
#include <vector>

#include "types/types.h"
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
       std::shared_ptr<types::Type> type);

  std::vector<std::shared_ptr<Var>> getArgVars() { return argVars; }
  std::vector<std::string> getArgNames() const { return argNames; }
  std::shared_ptr<Var> getArgVar(const std::string &name);

  void addVar(std::shared_ptr<Var> var) { vars.push_back(var); }

  std::vector<std::shared_ptr<BasicBlock>> getBlocks() { return blocks; }
  void addBlock(std::shared_ptr<BasicBlock> block) { blocks.push_back(block); }

  std::weak_ptr<IRModule> getModule() { return module; }
  void setModule(std::weak_ptr<IRModule> m) { module = m; }

  std::string referenceString() const override;
  std::string textRepresentation() const override;
};

} // namespace ir
} // namespace seq
