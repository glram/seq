#include <libgen.h>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <utility>
#include <vector>

#include "lang/seq.h"
#include "parser/ast/codegen/codegen.h"
#include "parser/ast/codegen/codegen_ctx.h"
#include "parser/ast/context.h"
#include "parser/common.h"
#include "parser/ocaml.h"

using fmt::format;
using std::make_pair;
using std::make_shared;
using std::pair;
using std::shared_ptr;
using std::string;
using std::vector;

namespace seq {
namespace ast {

CodegenContext::CodegenContext(shared_ptr<Cache> cache,
                               shared_ptr<seq::ir::BasicBlock> block,
                               shared_ptr<seq::ir::IRModule> module,
                               shared_ptr<seq::ir::Func> base,
                               shared_ptr<seq::SeqJIT> jit)
    : Context<CodegenItem>(""), cache(std::move(cache)), jit(std::move(jit)),
      module(std::move(module)) {
  stack.push_front(vector<string>());
  topBaseIndex = topBlockIndex = 0;
  if (block)
    addBlock(block, std::move(base));
}

shared_ptr<CodegenItem> CodegenContext::find(const string &name, bool onlyLocal,
                                             bool checkStdlib) const {
  auto i = Context<CodegenItem>::find(name);
  if (i)
    return i;
  return nullptr;
}

void CodegenContext::addVar(const string &name, shared_ptr<seq::ir::Var> v,
                            bool global) {
  auto i =
      make_shared<CodegenItem>(CodegenItem::Var, getBase(), global || isToplevel());
  i->var = std::move(v);
  add(name, i);
}

void CodegenContext::addType(const string &name, shared_ptr<seq::ir::types::Type> t,
                             bool global) {
  auto i =
      make_shared<CodegenItem>(CodegenItem::Type, getBase(), global || isToplevel());
  i->type = std::move(t);
  add(name, i);
}

void CodegenContext::addFunc(const string &name, shared_ptr<seq::ir::Func> f,
                             bool global) {
  auto i =
      make_shared<CodegenItem>(CodegenItem::Func, getBase(), global || isToplevel());
  i->func = std::move(f);
  add(name, i);
}

void CodegenContext::addBlock(shared_ptr<seq::ir::BasicBlock> newBlock,
                              shared_ptr<seq::ir::Func> newBase) {
  if (newBlock)
    topBlockIndex = blocks.size();
  blocks.push_back(newBlock);
  if (newBase) {
    topBaseIndex = bases.size();
    Context<CodegenItem>::addLevel();
  }
  bases.push_back(newBase);
}

void CodegenContext::replaceBlock(shared_ptr<seq::ir::BasicBlock> newBlock) {
  blocks.pop_back();
  blocks.push_back(newBlock);
}

void CodegenContext::popBlock() {
  // If we remove a function, we need to pop off all the variables
  if (bases.back())
    Context<CodegenItem>::removeLevel();
  bases.pop_back();
  topBaseIndex = bases.size() - 1;
  while (topBaseIndex && !bases[topBaseIndex])
    topBaseIndex--;
  blocks.pop_back();
  topBlockIndex = blocks.size() - 1;
  while (topBlockIndex && !blocks[topBlockIndex])
    topBlockIndex--;
}

// void CodegenContext::initJIT() {
//  jit = new seq::SeqJIT();
//  auto fn = new seq::Func();
//  fn->setName(".jit_0");
//
//  addBlock(fn->getBlock(), fn);
//  assert(topBaseIndex == topBlockIndex && topBlockIndex == 0);
//
//  execJIT();
//}
//
// void CodegenContext::execJIT(string varName, seq::Expr *varExpr) {
//   static int counter = 0;
//
//   assert(jit != nullptr);
//   assert(bases.size() == 1);
//   jit->addFunc((seq::Func *)bases[0]);
//
//   vector<pair<string, shared_ptr<CodegenItem>>> items;
//   for (auto &name : stack.front()) {
//     auto i = find(name);
//     if (i && i->isGlobal())
//       items.push_back(make_pair(name, i));
//   }
//   popBlock();
//   for (auto &i : items)
//     add(i.first, i.second);
//   if (varExpr) {
//     auto var = jit->addVar(varExpr);
//     add(varName, var);
//   }
//
//   // Set up new block
//   auto fn = new seq::Func();
//   fn->setName(format(".jit_{}", ++counter));
//   addBlock(fn->getBlock(), fn);
//   assert(topBaseIndex == topBlockIndex && topBlockIndex == 0);
//}

shared_ptr<seq::ir::types::Type> CodegenContext::realizeType(types::ClassTypePtr t) {
  t = t->getClass();
  seqassert(t, "type must be set and a class");
  seqassert(t->canRealize(), "{} must be realizable", t->toString());
  auto it = types.find(t->realizeString());
  if (it != types.end())
    return it->second;

  // LOG7("[codegen] generating ty {}", real.fullName);
  shared_ptr<seq::ir::types::Type> handle;
  vector<shared_ptr<seq::ir::types::Type>> types;
  vector<types::ClassTypePtr> typePtrs;
  vector<int> statics;
  for (auto &m : t->explicits)
    if (auto s = m.type->getStatic())
      statics.push_back(s->getValue());
    else {
      types.push_back(realizeType(m.type->getClass()));
      typePtrs.push_back(m.type->getClass());
    }
  auto name = t->name;
  if (name == ".void") {
    handle = seq::ir::types::kVoidType;
  } else if (name == ".bool") {
    handle = seq::ir::types::kBoolType;
  } else if (name == ".byte") {
    handle = seq::ir::types::kByteType;
  } else if (name == ".int") {
    handle = seq::ir::types::kIntType;
  } else if (name == ".float") {
    handle = seq::ir::types::kFloatType;
  } else if (name == ".str") {
    handle = seq::ir::types::kStringType;
  } else if (name == ".Int" || name == ".UInt") {
    assert(statics.size() == 1 && types.size() == 0);
    assert(statics[0] >= 1 && statics[0] <= 2048);
    handle = make_shared<seq::ir::types::IntNType>(statics[0], name == ".Int");
  } else if (name == ".Array") {
    assert(types.size() == 1 && statics.size() == 0);
    handle = make_shared<seq::ir::types::Array>(getPointer(typePtrs[0]));
  } else if (name == ".Ptr") {
    assert(types.size() == 1 && statics.size() == 0);
    if (types[0]->getId() == seq::ir::types::kByteType->getId())
      handle = seq::ir::types::kBytePointerType;
    else
      handle = make_shared<seq::ir::types::Pointer>(types[0]);
  } else if (name == ".Generator") {
    assert(types.size() == 1 && statics.size() == 0);
    handle = handle = make_shared<seq::ir::types::Generator>(types[0]);
  } else if (name == ".Optional") {
    assert(types.size() == 1 && statics.size() == 0);
    handle = make_shared<seq::ir::types::Optional>(getPointer(typePtrs[0]));
  } else if (startswith(name, ".Function.")) {
    types.clear();
    for (auto &m : t->args)
      types.push_back(realizeType(m->getClass()));
    auto ret = types[0];
    types.erase(types.begin());
    handle = make_shared<seq::ir::types::FuncType>(name, ret, types);
  } else if (startswith(name, ".Partial.")) {
    auto f = t->getCallable()->getClass();
    assert(f);
    auto callee = std::static_pointer_cast<seq::ir::types::FuncType>(realizeType(f));
    vector<shared_ptr<seq::ir::types::Type>> partials(f->args.size() - 1, nullptr);
    for (int i = 9; i < name.size(); i++)
      if (name[i] == '1')
        partials[i - 9] = realizeType(f->args[i - 9 + 1]->getClass());
    handle = make_shared<seq::ir::types::PartialFuncType>(name, callee, partials);
  } else {
    vector<string> names;
    vector<shared_ptr<seq::ir::types::Type>> types;
    shared_ptr<seq::ir::types::RecordType> record;

    if (t->isRecord()) {
      vector<string> names;
      vector<shared_ptr<seq::ir::types::Type>> types;
      handle = record = make_shared<seq::ir::types::RecordType>(name, types, names);
    } else {
      record = make_shared<seq::ir::types::RecordType>("", types, names);
      handle = make_shared<seq::ir::types::RefType>(name, record);
    }
    this->types[t->realizeString()] = handle;

    // Must do this afterwards to avoid infinite loop with recursive types
    for (auto &m : cache->memberRealizations[t->realizeString()]) {
      names.push_back(m.first);
      types.push_back(realizeType(m.second->getClass()));
    }
    record->setMemberNames(names);
    record->setMemberTypes(types);
  }
  return this->types[t->realizeString()] = handle;
}

shared_ptr<seq::ir::types::Pointer> CodegenContext::getPointer(types::ClassTypePtr t) {
  t = t->getClass();
  auto pointerName = fmt::format(FMT_STRING(".Ptr[{}]"), t->realizeString());
  auto it = types.find(pointerName);
  if (it != types.end())
    return std::static_pointer_cast<seq::ir::types::Pointer>(it->second);

  auto pointer = make_shared<seq::ir::types::Pointer>(realizeType(t));
  this->types[pointerName] = pointer;
  return pointer;
}

} // namespace ast
} // namespace seq
