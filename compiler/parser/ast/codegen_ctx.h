#pragma once

#include <deque>
#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "parser/ast/context.h"
#include "parser/common.h"
#include "sir/bblock.h"
#include "sir/func.h"
#include "sir/lvalue.h"
#include "sir/module.h"
#include "sir/operand.h"
#include "sir/rvalue.h"
#include "sir/trycatch.h"
#include "sir/types/types.h"
#include "sir/var.h"

namespace seq {
namespace ast {

namespace LLVMItem {

class Import;
class Var;
class Func;
class Class;

// TODO sharedct

class Item {
protected:
  std::shared_ptr<seq::ir::Func> base;
  bool global;
  std::unordered_set<std::string> attributes;

public:
  explicit Item(std::shared_ptr<seq::ir::Func> base, bool global = false)
      : base(std::move(base)), global(global) {}
  virtual ~Item() = default;

  std::shared_ptr<seq::ir::Func> getBase() const { return base; }
  bool isGlobal() const { return global; }
  bool hasAttr(const std::string &s) const {
    return attributes.find(s) != attributes.end();
  }

  virtual const Class *getClass() const { return nullptr; }
  virtual const Func *getFunc() const { return nullptr; }
  virtual const Var *getVar() const { return nullptr; }
  virtual const Import *getImport() const { return nullptr; }

  virtual std::shared_ptr<seq::ir::Lvalue> getLvalue() const = 0;
  virtual std::shared_ptr<seq::ir::Rvalue> getRvalue() const = 0;
  virtual std::shared_ptr<seq::ir::Operand> getOperand() const = 0;
};

class Var : public Item {
  std::shared_ptr<seq::ir::Var> handle;

public:
  Var(std::shared_ptr<seq::ir::Var> var, std::shared_ptr<seq::ir::Func> base,
      bool global = false)
      : Item(base, global), handle(std::move(var)) {}

  virtual std::shared_ptr<seq::ir::Lvalue> getLvalue() const {
    return std::make_shared<seq::ir::VarLvalue>(handle);
  }
  virtual std::shared_ptr<seq::ir::Rvalue> getRvalue() const {
    return std::make_shared<seq::ir::OperandRvalue>(getOperand());
  }
  virtual std::shared_ptr<seq::ir::Operand> getOperand() const {
    return std::make_shared<seq::ir::VarOperand>(handle);
  }

  const Var *getVar() const override { return this; }
  std::shared_ptr<seq::ir::Var> getHandle() const { return handle; }
};

class Func : public Item {
  std::shared_ptr<seq::ir::Func> handle;

public:
  Func(std::shared_ptr<seq::ir::Func> f, std::shared_ptr<seq::ir::Func> base,
       bool global = false)
      : Item(std::move(base), global), handle(std::move(f)) {}

  virtual std::shared_ptr<seq::ir::Lvalue> getLvalue() const {
    return std::make_shared<seq::ir::VarLvalue>(handle);
  }
  virtual std::shared_ptr<seq::ir::Rvalue> getRvalue() const {
    return std::make_shared<seq::ir::OperandRvalue>(getOperand());
  }
  virtual std::shared_ptr<seq::ir::Operand> getOperand() const {
    return std::make_shared<seq::ir::VarOperand>(handle);
  }

  const Func *getFunc() const override { return this; }
  std::shared_ptr<seq::ir::Func> getHandle() const { return handle; }
};

class Class : public Item {
  std::shared_ptr<seq::ir::types::Type> type;

public:
  Class(std::shared_ptr<seq::ir::types::Type> t,
        std::shared_ptr<seq::ir::Func> base, bool global = false)
      : Item(std::move(base), global), type(std::move(t)) {}

  std::shared_ptr<seq::ir::types::Type> getType() const { return type; }
  const Class *getClass() const override { return this; }

  virtual std::shared_ptr<seq::ir::Lvalue> getLvalue() const { assert(false); }
  virtual std::shared_ptr<seq::ir::Rvalue> getRvalue() const { assert(false); }
  virtual std::shared_ptr<seq::ir::Operand> getOperand() const {
    assert(false);
  }
};

class Import : public Item {
  std::string file;

public:
  Import(std::string file, std::shared_ptr<seq::ir::Func> base,
         bool global = false)
      : Item(std::move(base), global), file(std::move(file)) {}
  const Import *getImport() const override { return this; }
  std::string getFile() const { return file; }

  virtual std::shared_ptr<seq::ir::Lvalue> getLvalue() const { assert(false); }
  virtual std::shared_ptr<seq::ir::Rvalue> getRvalue() const { assert(false); }
  virtual std::shared_ptr<seq::ir::Operand> getOperand() const {
    assert(false);
  }
};
} // namespace LLVMItem

class LLVMContext : public Context<LLVMItem::Item> {
  std::vector<std::shared_ptr<seq::ir::Func>> bases;
  std::vector<std::shared_ptr<seq::ir::BasicBlock>> blocks;
  std::shared_ptr<seq::ir::IRModule> module;
  int topBlockIndex, topBaseIndex;

  std::shared_ptr<seq::SeqJIT> jit;

public:
  LLVMContext(const std::string &filename,
              std::shared_ptr<RealizationContext> realizations,
              std::shared_ptr<ImportContext> imports,
              std::shared_ptr<seq::ir::BasicBlock> block,
              std::shared_ptr<seq::ir::Func> base,
              std::shared_ptr<seq::ir::IRModule> module,
              std::shared_ptr<seq::SeqJIT> jit);
  virtual ~LLVMContext();

  std::shared_ptr<LLVMItem::Item> find(const std::string &name,
                                       bool onlyLocal = false,
                                       bool checkStdlib = true) const;

  using Context<LLVMItem::Item>::add;
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

  void initJIT();
  void execJIT(std::string varName = "",
               std::shared_ptr<seq::Expr> varExpr = nullptr);

  std::shared_ptr<seq::ir::types::Type> realizeType(types::ClassTypePtr t);
  // void dump(int pad = 0) override;

public:
  std::shared_ptr<seq::ir::IRModule> getModule() const { return module; }
  std::shared_ptr<seq::ir::Func> getBase() const { return bases[topBaseIndex]; }
  std::shared_ptr<seq::ir::BasicBlock> getBlock() const {
    return blocks[topBlockIndex];
  }
  bool isToplevel() const { return bases.size() == 1; }
  std::shared_ptr<seq::SeqJIT> getJIT() { return jit; }
  std::shared_ptr<seq::ir::types::Type> getType(const std::string &name) const {
    if (auto t = CAST(find(name), LLVMItem::Class))
      return t->getType();
    assert(false);
    return nullptr;
  }

public:
  static std::shared_ptr<LLVMContext>
  getContext(const std::string &file, std::shared_ptr<TypeContext> typeCtx,
             std::shared_ptr<seq::ir::IRModule> module);
};

} // namespace ast
} // namespace seq
