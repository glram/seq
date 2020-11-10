#pragma once

#include <deque>
#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "parser/ast/cache.h"
#include "parser/ast/context.h"
#include "parser/common.h"

#include "sir/bblock.h"
#include "sir/func.h"
#include "sir/module.h"
#include "sir/var.h"

#include "sir/types/types.h"

namespace seq {
namespace ast {

struct CodegenItem {
  enum Kind { Func, Type, Var } kind;
  std::shared_ptr<seq::ir::Func> base;
  bool global;
  std::unordered_set<std::string> attributes;

  // Union doesn't play nice with shared_ptr
  std::shared_ptr<seq::ir::Var> var;
  std::shared_ptr<seq::ir::Func> func;
  std::shared_ptr<seq::ir::types::Type> type;

public:
  CodegenItem(Kind k, std::shared_ptr<seq::ir::Func> base, bool global = false)
      : kind(k), base(std::move(base)), global(global) {}

  std::shared_ptr<seq::ir::Func> getBase() const { return base; }
  bool isGlobal() const { return global; }
  bool isVar() const { return kind == Var; }
  bool isFunc() const { return kind == Func; }
  bool isType() const { return kind == Type; }
  std::shared_ptr<seq::ir::Func> getFunc() const { return isFunc() ? func : nullptr; }
  std::shared_ptr<seq::ir::types::Type> getType() const {
    return isType() ? type : nullptr;
  }
  std::shared_ptr<seq::ir::Var> getVar() const { return isVar() ? var : nullptr; }
  bool hasAttr(const std::string &s) const {
    return attributes.find(s) != attributes.end();
  }
};

class CodegenContext : public Context<CodegenItem> {
  std::vector<std::shared_ptr<seq::ir::Func>> bases;
  std::vector<std::shared_ptr<seq::ir::BasicBlock>> blocks;
  std::shared_ptr<seq::ir::SIRModule> module;
  int topBlockIndex, topBaseIndex;

public:
  std::shared_ptr<Cache> cache;
  std::shared_ptr<seq::SeqJIT> jit;
  std::unordered_map<std::string, std::shared_ptr<seq::ir::types::Type>> types;
  std::unordered_map<std::string, std::pair<std::shared_ptr<seq::ir::Func>, bool>>
      functions;

public:
  CodegenContext(std::shared_ptr<Cache> cache,
                 std::shared_ptr<seq::ir::BasicBlock> block,
                 std::shared_ptr<seq::ir::SIRModule> module,
                 std::shared_ptr<seq::ir::Func> base, std::shared_ptr<seq::SeqJIT> jit);

  std::shared_ptr<CodegenItem> find(const std::string &name, bool onlyLocal = false,
                                    bool checkStdlib = true) const;

  using Context<CodegenItem>::add;
  void addVar(const std::string &name, std::shared_ptr<seq::ir::Var> v,
              bool global = false);
  void addType(const std::string &name, std::shared_ptr<seq::ir::types::Type> t,
               bool global = false);
  void addFunc(const std::string &name, std::shared_ptr<seq::ir::Func> f,
               bool global = false);
  void addImport(const std::string &name, const std::string &import,
                 bool global = false);
  void addBlock(std::shared_ptr<seq::ir::BasicBlock> newBlock = nullptr,
                std::shared_ptr<seq::ir::Func> newBase = nullptr);
  void popBlock() override;

  void replaceBlock(std::shared_ptr<seq::ir::BasicBlock> newBlock);

  // In Seq IR, vars are all defined at the function level. This override prevents
  // the context from getting rid of variables at the function level.
  void addBlock() override {}

  //  TODO
  //  void initJIT();
  //  void execJIT(std::string varName = "", seq::Expr varExpr = nullptr);

  std::shared_ptr<seq::ir::types::Type> realizeType(types::ClassTypePtr t);

public:
  std::shared_ptr<seq::ir::Func> getBase() const { return bases[topBaseIndex]; }
  std::shared_ptr<seq::ir::BasicBlock> getBlock() const {
    return blocks[topBlockIndex];
  }
  std::shared_ptr<seq::ir::SIRModule> getModule() const { return module; }
  bool isToplevel() const { return bases.size() == 1; }
  std::shared_ptr<seq::SeqJIT> getJIT() { return jit; }
  std::shared_ptr<seq::ir::types::Type> getType(const std::string &name) const {
    auto val = find(name);
    assert(val && val->getType());
    if (val)
      return val->getType();
    return nullptr;
  }

private:
  std::shared_ptr<seq::ir::types::PointerType> getPointer(types::ClassTypePtr t);
};

} // namespace ast
} // namespace seq
