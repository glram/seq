#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "types/types.h"
#include "var.h"

#include "codegen/codegen.h"

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
  std::weak_ptr<Func> enclosing;

  bool external;
  bool generator;

  bool internal;
  std::shared_ptr<types::Type> parent;
  std::string magicName;

public:
  Func(std::string name, std::vector<std::string> argNames,
       std::shared_ptr<types::Type> type);

  void setArgNames(std::vector<std::string> names);
  std::vector<std::string> getArgNames() const { return argNames; }

  void setType(std::shared_ptr<types::Type> type) override;
  std::vector<std::shared_ptr<Var>> getArgVars() { return argVars; }
  std::shared_ptr<Var> getArgVar(const std::string &name);

  void addVar(std::shared_ptr<Var> var) { vars.push_back(var); }
  std::vector<std::shared_ptr<Var>> getVars() { return vars; }

  std::vector<std::shared_ptr<BasicBlock>> getBlocks() { return blocks; }
  void addBlock(std::shared_ptr<BasicBlock> block) { blocks.push_back(block); }

  void setEnclosingFunc(std::weak_ptr<Func> f) { enclosing = std::move(f); }
  std::weak_ptr<Func> getEnclosingFunc() { return enclosing; }

  void setExternal() { external = true; }
  bool isExternal() const { return external; }

  void setGenerator() { generator = true; }
  bool isGenerator() const { return generator; }

  void setInternal(std::shared_ptr<types::Type> p, std::string n) {
    internal = true;
    parent = std::move(p);
    magicName = std::move(n);
  }
  bool isInternal() const { return internal; }
  std::shared_ptr<types::Type> getParentType() { return parent; }
  std::string getMagicName() const { return magicName; }

  std::string referenceString() const override;
  std::string textRepresentation() const override;

  bool isFunc() const override { return true; }

  void accept(codegen::CodegenVisitor &v, const std::string &nameOverride) override {
    v.visit(std::static_pointer_cast<Func>(getShared()), nameOverride);
  }
};

} // namespace ir
} // namespace seq
