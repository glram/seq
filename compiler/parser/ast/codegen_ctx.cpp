#include <libgen.h>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "lang/seq.h"
#include "parser/ast/codegen.h"
#include "parser/ast/codegen_ctx.h"
#include "parser/ast/context.h"
#include "parser/ast/format.h"
#include "parser/ast/transform.h"
#include "parser/ast/transform_ctx.h"
#include "parser/common.h"
#include "parser/ocaml.h"

#include "sir/func.h"

using fmt::format;
using std::make_pair;
using std::make_shared;
using std::pair;
using std::shared_ptr;
using std::string;
using std::vector;

namespace seq {
namespace ast {

string chop(const string &s) {
  return s.size() && s[0] == '.' ? s.substr(1) : s;
}

LLVMContext::LLVMContext(const string &filename,
                         shared_ptr<RealizationContext> realizations,
                         shared_ptr<ImportContext> imports,
                         std::shared_ptr<seq::ir::BasicBlock> block,
                         std::shared_ptr<seq::ir::Func> base,
                         std::shared_ptr<seq::ir::IRModule> module,
                         std::shared_ptr<seq::SeqJIT> jit)
    : Context<LLVMItem::Item>(filename, realizations, imports),
      module(std::move(module)), jit(jit) {
  stack.push_front(vector<string>());
  topBaseIndex = topBlockIndex = 0;
  if (block)
    addBlock(block, base);
}

LLVMContext::~LLVMContext() {}

shared_ptr<LLVMItem::Item> LLVMContext::find(const string &name, bool onlyLocal,
                                             bool checkStdlib) const {
  auto i = Context<LLVMItem::Item>::find(name);
  if (i && CAST(i, LLVMItem::Var) && onlyLocal)
    return (getBase() == i->getBase()) ? i : nullptr;
  if (i)
    return i;

  auto stdlib = imports->getImport("");
  if (stdlib && checkStdlib)
    return stdlib->lctx->find(name, onlyLocal, false);
  return nullptr;
}

void LLVMContext::addVar(const string &name, std::shared_ptr<seq::ir::Var> v,
                         bool global) {
  add(name, make_shared<LLVMItem::Var>(v, getBase(), global || isToplevel()));
}

void LLVMContext::addType(const string &name,
                          std::shared_ptr<seq::ir::types::Type> t,
                          bool global) {
  add(name, make_shared<LLVMItem::Class>(t, getBase(), global || isToplevel()));
}

void LLVMContext::addFunc(const string &name, std::shared_ptr<seq::ir::Func> f,
                          bool global) {
  add(name, make_shared<LLVMItem::Func>(f, getBase(), global || isToplevel()));
}

void LLVMContext::addImport(const string &name, const string &import,
                            bool global) {
  add(name,
      make_shared<LLVMItem::Import>(import, getBase(), global || isToplevel()));
}

void LLVMContext::addBlock(std::shared_ptr<seq::ir::BasicBlock> newBlock,
                           std::shared_ptr<seq::ir::Func> newBase) {
  Context<LLVMItem::Item>::addBlock();
  if (newBlock)
    topBlockIndex = blocks.size();
  blocks.push_back(newBlock);
  if (newBase)
    topBaseIndex = bases.size();
  bases.push_back(newBase);
}

void LLVMContext::popBlock() {
  Context<LLVMItem::Item>::popBlock();
  bases.pop_back();
  topBaseIndex = bases.size() - 1;
  while (topBaseIndex && !bases[topBaseIndex])
    topBaseIndex--;
  blocks.pop_back();
  topBlockIndex = blocks.size() - 1;
  while (topBlockIndex && !blocks[topBlockIndex])
    topBlockIndex--;
  Context<LLVMItem::Item>::popBlock();
}

void LLVMContext::initJIT() { throw std::runtime_error("not implemented"); }

void LLVMContext::execJIT(string varName, std::shared_ptr<seq::Expr> varExpr) {
  // static int counter = 0;

  // assert(jit != nullptr);
  // assert(bases.size() == 1);
  // jit->addFunc(( std::shared_ptr<seq::Func> )bases[0]);

  // vector<pair<string, shared_ptr<LLVMItem::Item>>> items;
  // for (auto &name : stack.front()) {
  //   auto i = find(name);
  //   if (i && i->isGlobal())
  //     items.push_back(make_pair(name, i));
  // }
  // popBlock();
  // for (auto &i : items)
  //   add(i.first, i.second);
  // if (varExpr) {
  //   auto var = jit->addVar(varExpr);
  //   add(varName, var);
  // }

  // // Set up new block
  // auto fn = new seq::Func();
  // fn->setName(format("$jit_{}", ++counter));
  // addBlock(fn->getBlock(), fn);
  // assert(topBaseIndex == topBlockIndex && topBlockIndex == 0);
  throw std::runtime_error("not implemented");
}

// void LLVMContext::dump(int pad) {
//   auto ordered = decltype(map)(map.begin(), map.end());
//   for (auto &i : ordered) {
//     std::string s;
//     auto t = i.second.top();
//     if (auto im = t->getImport()) {
//       DBG("{}{:.<25} {}", string( std::shared_ptr<pad> 2, ' '), i.first,
//       '<import>'); getImports()->getImport(im->getFile())->tctx->dump(pad+1);
//     }
//     else
//       DBG("{}{:.<25} {}", string( std::shared_ptr<pad> 2, ' '), i.first,
//       t->getType()->toString(true));
//   }
// }

std::shared_ptr<seq::ir::types::Type>
LLVMContext::realizeType(types::ClassTypePtr t) {
  // LOG7("[codegen] looking for ty {} / {}", t->name, t->toString(true));
  assert(t && t->canRealize());
  auto it = getRealizations()->classRealizations.find(t->name);
  assert(it != getRealizations()->classRealizations.end());
  auto it2 = it->second.find(t->realizeString(t->name, false));
  assert(it2 != it->second.end());
  auto &real = it2->second;
  if (real.handle)
    return real.handle->getShared();

  // LOG7("[codegen] generating ty {}", real.fullName);

  // TODO - reuse types with same name
  vector<std::shared_ptr<seq::ir::types::Type>> types;
  vector<int> statics;
  for (auto &m : t->explicits)
    if (auto s = m.type->getStatic())
      statics.push_back(s->getValue());
    else
      types.push_back(realizeType(m.type->getClass()));
  auto name = chop(t->name);
  if (name == "str") {
    real.handle = seq::ir::types::kStringType;
  } else if (name == "Int" || name == "UInt") {
    assert(statics.size() == 1 && types.size() == 0);
    assert(statics[0] >= 1 && statics[0] <= 2048);
    real.handle = seq::ir::types::kIntType;
  } else if (name == "array") {
    assert(types.size() == 1 && statics.size() == 0);
    real.handle =
        std::make_shared<seq::ir::types::Array>(types[0]->getShared());
  } else if (name == "ptr") {
    assert(types.size() == 1 && statics.size() == 0);
    real.handle =
        std::make_shared<seq::ir::types::Pointer>(types[0]->getShared());
  } else if (name == "generator") {
    assert(types.size() == 1 && statics.size() == 0);
    real.handle =
        std::make_shared<seq::ir::types::Generator>(types[0]->getShared());
  } else if (name == "optional") {
    assert(types.size() == 1 && statics.size() == 0);
    real.handle =
        std::make_shared<seq::ir::types::Optional>(types[0]->getShared());
  } else if (name.substr(0, 9) == "function.") {
    types.clear();
    for (auto &m : t->args)
      types.push_back(realizeType(m->getClass()));
    auto ret = types[0];
    types.erase(types.begin());
    real.handle = seq::types::FuncType::get(types, ret);
  } else if (name.substr(0, 8) == "partial.") {
    auto f = t->getCallable();
    assert(f);
    auto callee = realizeType(f);
    vector<std::shared_ptr<seq::ir::types::Type>> partials(f->args.size() - 1,
                                                           nullptr);
    auto p = std::dynamic_pointer_cast<types::PartialType>(t);
    assert(p);
    for (int i = 0; i < p->knownTypes.size(); i++)
      if (p->knownTypes[i])
        partials[i] = realizeType(f->args[i + 1]->getClass());
    real.handle = seq::types::PartialFuncType::get(callee, partials);
  } else {
    vector<string> names;
    vector<std::shared_ptr<seq::ir::types::Type>> types;
    for (auto &m : real.args) {
      names.push_back(m.first);
      types.push_back(realizeType(m.second)->getShared());
    }
    if (t->isRecord()) {
      vector<string> x;
      for (auto &t : types)
        x.push_back(t->getName());
      if (name.substr(0, 6) == "tuple.")
        name = "";
      real.handle = std::make_shared<seq::ir::types::Type>(types, names, name);
    } else {
      real.handle = std::make_shared<seq::ir::types::Reference>(
          std::make_shared<seq::ir::types::Type>(types, names, name));
    }
  }
  // LOG7("{} -> {} -> {}", t->toString(), t->realizeString(t->name, false),
  // real.handle->getName());
  return real.handle->getShared();
}

shared_ptr<LLVMContext>
LLVMContext::getContext(const string &file, shared_ptr<TypeContext> typeCtx,
                        std::shared_ptr<seq::ir::IRModule> module) {
  auto realizations = typeCtx->getRealizations();
  auto imports = typeCtx->getImports();
  auto stdlib = const_cast<ImportContext::Import *>(imports->getImport(""));

  auto base = module->getBase();
  auto block = base->getBlocks()[0];
  stdlib->lctx = make_shared<LLVMContext>(
      stdlib->filename, realizations, imports, block, base, module, nullptr);
  // Now add all realization stubs
  for (auto &ff : realizations->classRealizations)
    for (auto &f : ff.second) {
      auto &real = f.second;
      stdlib->lctx->realizeType(real.type);
      stdlib->lctx->addType(real.fullName, real.handle->getShared());
    }
  for (auto &ff : realizations->funcRealizations)
    for (auto &f : ff.second) {
      // Realization: f.second
      auto &real = f.second;
      auto ast = real.ast;
      // TODO fix internals
      //      if (in(ast->attributes, "internal")) {
      //        // LOG7("[codegen] generating internal fn {} ~ {}",
      //        real.fullName,
      //        // ast->name);
      //        vector<std::shared_ptr<seq::ir::types::Type>> types;
      //
      //        // static: has self as arg
      //        assert(real.type->parent && real.type->parent->getClass());
      //        std::shared_ptr<seq::ir::types::Type> typ =
      //            stdlib->lctx->realizeType(real.type->parent->getClass());
      //        int startI = 1;
      //        if (ast->args.size() && ast->args[0].name == "self")
      //          startI = 2;
      //        for (int i = startI; i < real.type->args.size(); i++)
      //          types.push_back(
      //              stdlib->lctx->realizeType(real.type->args[i]->getClass())
      //                  ->getShared());
      //        real.handle = typ->findMagic(ast->name, types);
      //      } else {
      // LOG7("[codegen] generating fn stub {}", real.fullName);
      real.handle =
          std::make_shared<seq::ir::Func>("unnamed", std::vector<std::string>(),
                                          seq::ir::types::kNoArgVoidFuncType);
      // }
      stdlib->lctx->addFunc(real.fullName, real.handle);
    }

  CodegenVisitor c(stdlib->lctx);
  c.transform(stdlib->statements.get());

  auto def = const_cast<ImportContext::Import *>(imports->getImport(file));
  assert(def);
  def->lctx = make_shared<LLVMContext>(file, realizations, imports, block, base,
                                       module, nullptr);
  return def->lctx;
}

} // namespace ast
} // namespace seq
