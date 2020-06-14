#pragma once

#include <deque>
#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "parser/ast/context.h"
#include "parser/common.h"
#include "sir/func.h"
#include "sir/types/types.h"
#include "sir/var.h"
#include "sir/lvalue.h"
#include "sir/rvalue.h"
#include "sir/operand.h"
#include "sir/bblock.h"
#include "sir/trycatch.h"
#include "sir/module.h"

namespace seq {
namespace ast {

namespace LLVMItem {

class Import;
class Var;
class Func;
class Class;

class Item {
protected:
  seq::ir::Func *base;
  bool global;
  std::unordered_set<std::string> attributes;

public:
  Item(seq::ir::Func *base, bool global = false) : base(base), global(global) {}
  virtual ~Item() {}

  const seq::ir::Func *getBase() const { return base; }
  bool isGlobal() const { return global; }
  bool hasAttr(const std::string &s) const {
    return attributes.find(s) != attributes.end();
  }

  virtual const Class *getClass() const { return nullptr; }
  virtual const Func *getFunc() const { return nullptr; }
  virtual const Var *getVar() const { return nullptr; }
  virtual const Import *getImport() const { return nullptr; }

  virtual seq::ir::Lvalue *getLvalue() const = 0;
  virtual seq::ir::Rvalue *getRvalue() const = 0;
  virtual seq::ir::Operand *getOperand() const = 0;
};

class Var : public Item {
  seq::ir::Var *handle;

public:
  Var(seq::ir::Var *var, seq::ir::Func *base, bool global = false)
      : Item(base, global), handle(var) {}

  virtual seq::ir::Lvalue *getLvalue() const { return new seq::ir::VarLvalue(handle->getShared()); }
  virtual seq::ir::Rvalue *getRvalue() const { return new seq::ir::OperandRvalue(getOperand()->getShared()); }
  virtual seq::ir::Operand *getOperand() const { return new seq::ir::VarOperand(handle->getShared()); }

  const Var *getVar() const override { return this; }
  seq::ir::Var *getHandle() const { return handle; }
};

class Func : public Item {
  seq::ir::Func *handle;

public:
  Func(seq::ir::Func *f, seq::ir::Func *base, bool global = false)
      : Item(base, global), handle(f) {}

  virtual seq::ir::Lvalue *getLvalue() const { return new seq::ir::VarLvalue(handle->getShared()); }
  virtual seq::ir::Rvalue *getRvalue() const { return new seq::ir::OperandRvalue(getOperand()->getShared()); }
  virtual seq::ir::Operand *getOperand() const { return new seq::ir::VarOperand(handle->getShared()); }

  const Func *getFunc() const override { return this; }
  seq::ir::Func *getHandle() const { return handle; }
};

class Class : public Item {
  seq::ir::types::Type *type;

public:
  Class(seq::ir::types::Type *t, seq::ir::Func *base, bool global = false)
      : Item(base, global), type(t) {}

  seq::ir::types::Type *getType() const { return type; }
  const Class *getClass() const override { return this; }

  virtual seq::ir::Lvalue *getLvalue() const { assert(false); }
  virtual seq::ir::Rvalue *getRvalue() const { assert(false); }
  virtual seq::ir::Operand *getOperand() const { assert(false); }
};

class Import : public Item {
  std::string file;

public:
  Import(const std::string &file, seq::ir::Func *base, bool global = false)
      : Item(base, global), file(file) {}
  const Import *getImport() const override { return this; }
  std::string getFile() const { return file; }

  virtual seq::ir::Lvalue *getLvalue() const { assert(false); }
  virtual seq::ir::Rvalue *getRvalue() const { assert(false); }
  virtual seq::ir::Operand *getOperand() const { assert(false); }
};
} // namespace LLVMItem

class LLVMContext : public Context<LLVMItem::Item> {
  std::vector<seq::ir::Func *> bases;
  std::vector<seq::ir::BasicBlock *> blocks;
  int topBlockIndex, topBaseIndex;

  seq::ir::TryCatch *tryCatch;
  seq::SeqJIT *jit;

public:
  LLVMContext(const std::string &filename,
              std::shared_ptr<RealizationContext> realizations,
              std::shared_ptr<ImportContext> imports, seq::ir::BasicBlock *block,
              seq::ir::Func *base, seq::SeqJIT *jit);
  virtual ~LLVMContext();

  std::shared_ptr<LLVMItem::Item> find(const std::string &name,
                                       bool onlyLocal = false,
                                       bool checkStdlib = true) const;

  using Context<LLVMItem::Item>::add;
  void addVar(const std::string &name, seq::ir::Var *v, bool global = false);
  void addType(const std::string &name, seq::ir::types::Type *t,
               bool global = false);
  void addFunc(const std::string &name, seq::ir::Func *f, bool global = false);
  void addImport(const std::string &name, const std::string &import,
                 bool global = false);
  void addBlock(seq::ir::BasicBlock *newBlock = nullptr,
                seq::ir::Func *newBase = nullptr);
  void popBlock() override;

  void initJIT();
  void execJIT(std::string varName = "", seq::Expr *varExpr = nullptr);

  // void dump(int pad = 0) override;

public:
  seq::ir::Func *getBase() const { return bases[topBaseIndex]; }
  seq::ir::BasicBlock *getBlock() const { return blocks[topBlockIndex]; }
  seq::ir::TryCatch *getTryCatch() const { return tryCatch; }
  void setTryCatch(seq::ir::TryCatch *t) { tryCatch = t; }
  bool isToplevel() const { return bases.size() == 1; }
  seq::SeqJIT *getJIT() { return jit; }
  seq::ir::types::Type *getType(const std::string &name) const {
    if (auto t = CAST(find(name), LLVMItem::Class))
      return t->getType();
    assert(false);
    return nullptr;
  }

public:
  static std::shared_ptr<LLVMContext>
  getContext(const std::string &file, std::shared_ptr<TypeContext> typeCtx,
             seq::ir::IRModule *module);
};

} // namespace ast
} // namespace seq
