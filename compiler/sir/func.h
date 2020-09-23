#pragma once

#include <memory>
#include <string>
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

  std::vector<std::shared_ptr<BasicBlock>> getBlocks() { return blocks; }
  void addBlock(std::shared_ptr<BasicBlock> block) { blocks.push_back(block); }

  void setEnclosingFunc(std::weak_ptr<Func> f) { enclosing = f; }

  void setExternal() { external = true; }
  bool isExternal() const { return external; }

  void setGenerator() { generator = true; }
  bool isGenerator() const { return generator; }

  void setInternal(std::shared_ptr<types::Type> p, std::string n) {
    internal = true;
    parent = std::move(p);
    magicName = std::move(n);
  }

  std::string referenceString() const override;
  std::string textRepresentation() const override;

  void accept(codegen::CodegenVisitor &v) override {
    v.visit(std::static_pointer_cast<Func>(getShared()));
  }
};

} // namespace ir
} // namespace seq
