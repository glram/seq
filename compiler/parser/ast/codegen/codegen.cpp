#include "util/fmt/format.h"
#include "util/fmt/ostream.h"
#include <memory>
#include <ostream>
#include <stack>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "sir/base.h"
#include "sir/instr.h"
#include "sir/terminator.h"
#include "sir/trycatch.h"

#include "parser/ast/ast.h"
#include "parser/ast/codegen/codegen.h"
#include "parser/ast/codegen/codegen_ctx.h"
#include "parser/common.h"

using fmt::format;
using std::get;
using std::make_shared;
using std::make_unique;
using std::move;
using std::ostream;
using std::shared_ptr;
using std::stack;
using std::string;
using std::unique_ptr;
using std::unordered_map;
using std::unordered_set;
using std::vector;
using std::weak_ptr;

using namespace seq::ir;

namespace seq {
namespace ast {

void CodegenVisitor::defaultVisit(const Expr *n) {
  seqassert(false, "invalid node {}", *n);
}

void CodegenVisitor::defaultVisit(const Stmt *n) {
  seqassert(false, "invalid node {}", *n);
}

void CodegenVisitor::defaultVisit(const Pattern *n) {
  seqassert(false, "invalid node {}", *n);
}

shared_ptr<BasicBlock> CodegenVisitor::newBlock() {
  auto ret = make_shared<BasicBlock>();

  ctx->getBase()->addBlock(ret);

  auto templ = ctx->getBlock();
  if (templ->getAttribute(kLoopAttribute))
    ret->setAttribute(kLoopAttribute, make_shared<LoopAttribute>(
                                          *std::static_pointer_cast<LoopAttribute>(
                                              templ->getAttribute(kLoopAttribute))));
  if (templ->getAttribute(kTryCatchAttribute))
    ret->setAttribute(
        kTryCatchAttribute,
        make_shared<TryCatchAttribute>(*std::static_pointer_cast<TryCatchAttribute>(
            templ->getAttribute(kTryCatchAttribute))));
  return ret;
}

void CodegenVisitor::condSetTerminator(shared_ptr<Terminator> term) {
  if (!ctx->getBlock()->getTerminator())
    ctx->getBlock()->setTerminator(std::move(term));
}

shared_ptr<seq::ir::Operand> CodegenVisitor::toOperand(const CodegenResult res) {
  switch (res.tag) {
  case CodegenResult::OP:
    return res.operandResult;
  case CodegenResult::LVALUE:
    seqassert(false, "cannot convert lvalue to operand.");
    return nullptr;
  case CodegenResult::RVALUE: {
    auto t = make_shared<ir::Var>(res.typeOverride ? res.typeOverride
                                                   : res.rvalueResult->getType());
    ctx->getBase()->addVar(t);
    ctx->getBlock()->add(
        make_shared<AssignInstr>(make_shared<VarLvalue>(t), res.rvalueResult));
    return make_shared<VarOperand>(t);
  }
  case CodegenResult::PATTERN:
    seqassert(false, "cannot convert pattern to operand.");
    return nullptr;
  case CodegenResult::TYPE:
    seqassert(false, "cannot convert pattern to operand.");
    return nullptr;
  default:
    seqassert(false, "cannot convert unknown to operand.");
    return nullptr;
  }
}

shared_ptr<seq::ir::Rvalue> CodegenVisitor::toRvalue(const CodegenResult res) {
  switch (res.tag) {
  case CodegenResult::OP:
    return make_shared<OperandRvalue>(res.operandResult);
  case CodegenResult::LVALUE:
    seqassert(false, "cannot convert lvalue to rvalue.");
    return nullptr;
  case CodegenResult::RVALUE:
    return res.rvalueResult;
  case CodegenResult::PATTERN:
    seqassert(false, "cannot convert pattern to rvalue.");
    return nullptr;
  case CodegenResult::TYPE:
    seqassert(false, "cannot convert pattern to rvalue.");
    return nullptr;
  default:
    seqassert(false, "cannot convert unknown to rvalue.");
    return nullptr;
  }
}

CodegenVisitor::CodegenVisitor(shared_ptr<CodegenContext> ctx) : ctx(std::move(ctx)) {}

CodegenResult CodegenVisitor::transform(const ExprPtr &expr) {
  if (!expr)
    return CodegenResult();
  CodegenVisitor v(ctx);
  v.setSrcInfo(expr->getSrcInfo());
  expr->accept(v);
  v.result.addAttribute(kSrcInfoAttribute,
                        make_shared<SrcInfoAttribute>(v.getSrcInfo()));
  return v.result;
}

CodegenResult CodegenVisitor::transform(const StmtPtr &stmt) {
  CodegenVisitor v(ctx);
  stmt->accept(v);
  v.setSrcInfo(stmt->getSrcInfo());
  v.result.addAttribute(kSrcInfoAttribute,
                        make_shared<SrcInfoAttribute>(v.getSrcInfo()));
  return v.result;
}

CodegenResult CodegenVisitor::transform(const PatternPtr &ptr) {
  CodegenVisitor v(ctx);
  v.setSrcInfo(ptr->getSrcInfo());
  ptr->accept(v);
  v.result.addAttribute(kSrcInfoAttribute,
                        std::make_shared<SrcInfoAttribute>(v.getSrcInfo()));
  return v.result;
}

shared_ptr<seq::ir::IRModule> CodegenVisitor::apply(shared_ptr<Cache> cache,
                                                    StmtPtr stmts) {
  auto module = make_shared<IRModule>("module");
  auto block = module->getBase()->getBlocks()[0];
  auto ctx =
      make_shared<CodegenContext>(cache, block, module, module->getBase(), nullptr);

  // Now add all realization stubs
  for (auto &ff : cache->realizations)
    for (auto &f : ff.second) {
      auto t = ctx->realizeType(f.second->getClass());
      ctx->addType(f.first, t);
    }
  for (auto &ff : cache->realizations)
    for (auto &f : ff.second)
      if (auto t = f.second->getFunc()) {
        auto ast = (FunctionStmt *)(cache->asts[ff.first].get());
        auto names = split(ast->name, '.');
        auto name = names.back();
        if (in(ast->attributes, "internal")) {
          vector<shared_ptr<ir::types::Type>> types;
          auto p = t->codegenParent ? t->codegenParent : t->parent;
          seqassert(p && p->getClass(), "parent must be set ({})",
                    p ? p->toString() : "-");
          shared_ptr<ir::types::Type> typ = ctx->realizeType(p->getClass());
          int startI = 1;
          if (ast->args.size() && ast->args[0].name == "self")
            startI = 2;
          for (int i = startI; i < t->args.size(); i++)
            types.push_back(ctx->realizeType(t->args[i]->getClass()));
          if (isdigit(name[0])) // TODO: get rid of this hack
            name = names[names.size() - 2];
          LOG7("[codegen] generating internal fn {} -> {}", ast->name, name);
          auto fn = make_shared<ir::Func>(names.back(), std::vector<std::string>(),
                                          ir::types::kNoArgVoidFuncType);
          fn->setInternal(typ, name);
          // ctx->functions[f.first] = {typ->findMagic(name, types), true};
          ctx->functions[f.first] = {fn, false};
          ctx->getModule()->addGlobal(fn);
        } else {
          auto fn = make_shared<ir::Func>(name, std::vector<std::string>(),
                                          ir::types::kNoArgVoidFuncType);
          ctx->functions[f.first] = {fn, false};
          ctx->getModule()->addGlobal(fn);
        }
        ctx->addFunc(f.first, ctx->functions[f.first].first);
      }
  CodegenVisitor(ctx).transform(stmts);
  return module;
}

void CodegenVisitor::visit(const BoolExpr *expr) {
  result = CodegenResult(make_shared<LiteralOperand>(expr->value));
}

void CodegenVisitor::visit(const IntExpr *expr) {
  if (!expr->sign)
    result = CodegenResult(make_shared<LiteralOperand>(uint64_t(expr->intValue)));
  else
    result = CodegenResult(make_shared<LiteralOperand>(expr->intValue));
}

void CodegenVisitor::visit(const FloatExpr *expr) {
  result = CodegenResult(make_shared<LiteralOperand>(expr->value));
}

void CodegenVisitor::visit(const StringExpr *expr) {
  result = CodegenResult(make_shared<LiteralOperand>(expr->value));
}

void CodegenVisitor::visit(const IdExpr *expr) {
  auto val = ctx->find(expr->value);
  seqassert(val, "cannot find '{}'", expr->value);
  // TODO: this makes no sense: why setAtomic on temporary expr?
  // if (var->isGlobal() && var->getBase() == ctx->getBase() &&
  //     ctx->hasFlag("atomic"))
  //   dynamic_cast<seq::VarExpr *>(i->getExpr())->setAtomic();
  //

  // TODO verify
  if (auto v = val->getVar())
    result = CodegenResult(make_shared<VarOperand>(v));
  else if (auto f = val->getFunc())
    result = CodegenResult(make_shared<VarOperand>(f));
  else
    result = CodegenResult(val->getType());
}

void CodegenVisitor::visit(const IfExpr *expr) {
  auto var = make_shared<ir::Var>(realizeType(expr->getType()->getClass()));
  ctx->getBase()->addVar(var);

  auto tBlock = newBlock();
  auto fBlock = newBlock();
  auto nextBlock = newBlock();

  auto condResultOp = toOperand(transform(expr->cond));
  ctx->getBlock()->setTerminator(
      make_shared<CondJumpTerminator>(tBlock, fBlock, condResultOp));

  ctx->addBlock(tBlock);
  auto tResultOp = toOperand(transform(expr->eif));
  ctx->getBlock()->add(make_shared<AssignInstr>(make_shared<VarLvalue>(var),
                                                make_shared<OperandRvalue>(tResultOp)));
  ctx->getBlock()->setTerminator(make_shared<JumpTerminator>(nextBlock));
  ctx->popBlock();

  ctx->addBlock(fBlock);
  auto fResultOp = toOperand(transform(expr->eelse));
  ctx->getBlock()->add(make_shared<AssignInstr>(make_shared<VarLvalue>(var),
                                                make_shared<OperandRvalue>(fResultOp)));
  ctx->getBlock()->setTerminator(make_shared<JumpTerminator>(nextBlock));
  ctx->popBlock();

  ctx->replaceBlock(nextBlock);

  result = CodegenResult(make_shared<VarOperand>(var));
}

void CodegenVisitor::visit(const BinaryExpr *expr) {
  assert(expr->op == "&&" || expr->op == "||");

  auto var = make_shared<ir::Var>(ir::types::kBoolType);
  ctx->getBase()->addVar(var);

  auto trueBlock = newBlock();
  auto falseBlock = newBlock();
  auto nextBlock = newBlock();

  auto lhsOp = toOperand(transform(expr->lexpr));
  if (expr->op == "&&") {
    auto tLeftBlock = newBlock();
    ctx->getBlock()->setTerminator(
        make_shared<CondJumpTerminator>(tLeftBlock, falseBlock, lhsOp));

    ctx->addBlock(tLeftBlock);
    auto rhsOp = toOperand(transform(expr->rexpr));
    ctx->getBlock()->setTerminator(
        make_shared<CondJumpTerminator>(trueBlock, falseBlock, rhsOp));
    ctx->popBlock();
  } else {
    auto fLeftBlock = newBlock();
    ctx->getBlock()->setTerminator(
        make_shared<CondJumpTerminator>(trueBlock, fLeftBlock, lhsOp));

    ctx->addBlock(fLeftBlock);
    auto rhsOp = toOperand(transform(expr->rexpr));
    ctx->getBlock()->setTerminator(
        make_shared<CondJumpTerminator>(trueBlock, falseBlock, rhsOp));
    ctx->popBlock();
  }

  ctx->addBlock(trueBlock);
  auto trueOperand = make_shared<LiteralOperand>(true);
  ctx->getBlock()->add(make_shared<AssignInstr>(
      make_shared<VarLvalue>(var), make_shared<OperandRvalue>(trueOperand)));
  ctx->getBlock()->setTerminator(make_shared<JumpTerminator>(nextBlock));
  ctx->popBlock();

  ctx->addBlock(falseBlock);
  auto falseOperand = make_shared<LiteralOperand>(false);
  ctx->getBlock()->add(make_shared<AssignInstr>(
      make_shared<VarLvalue>(var), make_shared<OperandRvalue>(falseOperand)));
  ctx->getBlock()->setTerminator(make_shared<JumpTerminator>(nextBlock));
  ctx->popBlock();

  ctx->replaceBlock(nextBlock);

  result = CodegenResult(make_shared<VarOperand>(var));
}

void CodegenVisitor::visit(const PipeExpr *expr) {
  vector<shared_ptr<Operand>> ops;
  vector<bool> parallel;
  for (const auto &item : expr->items) {
    ops.push_back(toOperand(transform(item.expr)));
    parallel.push_back(item.op == "||>");
  }
  result = CodegenResult(make_shared<PipelineRvalue>(ops, parallel));
}

void CodegenVisitor::visit(const CallExpr *expr) {
  // TODO fix partial call
  auto lhs = toOperand(transform(expr->expr));
  vector<shared_ptr<Operand>> items;
  bool isPartial = false;
  for (auto &&i : expr->args) {
    if (CAST(i.value, EllipsisExpr)) {
      items.push_back(nullptr);
      isPartial = true;
    } else {
      items.push_back(toOperand(transform(i.value)));
    }
  }
  if (isPartial)
    result = CodegenResult(make_shared<PartialCallRValue>(
        lhs, items,
        std::static_pointer_cast<ir::types::PartialFuncType>(
            realizeType(expr->getType()->getClass()))));
  else {
    result = CodegenResult(make_shared<CallRValue>(lhs, items));
    result.typeOverride = realizeType(expr->getType()->getClass());
  }
}

void CodegenVisitor::visit(const StackAllocExpr *expr) {
  auto arrayType = std::static_pointer_cast<ir::types::Array>(
      realizeType(expr->getType()->getClass()));
  result = CodegenResult(
      std::make_shared<StackAllocRvalue>(arrayType, toOperand(transform(expr->expr))));
}
void CodegenVisitor::visit(const DotExpr *expr) {
  result = CodegenResult(
      make_shared<MemberRvalue>(toOperand(transform(expr->expr)), expr->member));
}

void CodegenVisitor::visit(const PtrExpr *expr) {
  auto e = CAST(expr->expr, IdExpr);
  assert(e);
  auto v = ctx->find(e->value, true)->getVar();
  assert(v);
  result = CodegenResult(
      make_shared<VarPointerOperand>(realizeType(expr->getType()->getClass()), v));
}

void CodegenVisitor::visit(const YieldExpr *expr) {
  ctx->getBase()->setGenerator();

  auto var = make_shared<ir::Var>(realizeType(expr->getType()->getClass()));
  ctx->getBase()->addVar(var);

  auto dst = newBlock();
  ctx->getBlock()->setTerminator(make_shared<YieldTerminator>(dst, nullptr, var));
  ctx->replaceBlock(dst);

  result = CodegenResult(make_shared<VarOperand>(var));
}

void CodegenVisitor::visit(const SuiteStmt *stmt) {
  for (auto &s : stmt->stmts) {
    transform(s);
  }
}

void CodegenVisitor::visit(const PassStmt *stmt) {}

void CodegenVisitor::visit(const BreakStmt *stmt) {
  auto loop = std::static_pointer_cast<LoopAttribute>(
      ctx->getBlock()->getAttribute(kLoopAttribute));
  auto dst = loop->end.lock();
  if (!dst)
    seqassert(false, "No loop end");
  ctx->getBlock()->setTerminator(make_shared<JumpTerminator>(dst));
}

void CodegenVisitor::visit(const ContinueStmt *stmt) {
  auto loop = std::static_pointer_cast<LoopAttribute>(
      ctx->getBlock()->getAttribute(kLoopAttribute));
  auto dst = loop->cond.lock();
  if (!dst)
    dst = loop->begin.lock();
  if (!dst)
    seqassert(false, "No loop body");
  ctx->getBlock()->setTerminator(make_shared<JumpTerminator>(dst));
}

void CodegenVisitor::visit(const ExprStmt *stmt) {
  auto rval = toRvalue(transform(stmt->expr));
  ctx->getBlock()->add(make_shared<RvalueInstr>(rval));
}

void CodegenVisitor::visit(const AssignStmt *stmt) {
  /// TODO: atomic operations & JIT
  auto i = CAST(stmt->lhs, IdExpr);
  assert(i);
  auto var = i->value;

  if (!stmt->rhs) {
    assert(var == ".__argv__");
    ctx->addVar(var, ctx->getModule()->getArgVar());
  } else if (stmt->rhs->isType()) {
    // ctx->addType(var, realizeType(stmt->rhs->getType()->getClass()));
  } else {
    auto v = make_shared<ir::Var>(var, realizeType(stmt->rhs->getType()->getClass()));
    if (var[0] == '.')
      ctx->getModule()->addGlobal(v);
    else
      ctx->getBase()->addVar(v);
    ctx->addVar(var, v);
    auto rval = toRvalue(transform(stmt->rhs));
    ctx->getBlock()->add(make_shared<AssignInstr>(make_shared<VarLvalue>(v), rval));
  }
}

void CodegenVisitor::visit(const AssignMemberStmt *stmt) {
  auto i = CAST(stmt->lhs, IdExpr);
  auto name = i->value;
  auto var = ctx->find(name, false);
  auto rhs = toRvalue(transform(stmt->rhs));
  ctx->getBlock()->add(
      make_shared<AssignInstr>(make_shared<VarMemberLvalue>(var->getVar(), name), rhs));
}

void CodegenVisitor::visit(const UpdateStmt *stmt) {
  auto i = CAST(stmt->lhs, IdExpr);
  assert(i);
  auto var = i->value;
  auto val = ctx->find(var, true);
  assert(val && val->getVar());
  auto rhs = toRvalue(transform(stmt->rhs));
  ctx->getBlock()->add(
      make_shared<AssignInstr>(make_shared<VarLvalue>(val->getVar()), rhs));
}

void CodegenVisitor::visit(const DelStmt *stmt) {
  auto expr = CAST(stmt->expr, IdExpr);
  assert(expr);
  auto v = ctx->find(expr->value, true)->getVar();
  assert(v);
  ctx->remove(expr->value);
}

void CodegenVisitor::visit(const ReturnStmt *stmt) {
  auto retVal = stmt->expr ? toOperand(transform(stmt->expr)) : nullptr;
  ctx->getBlock()->setTerminator(make_shared<ReturnTerminator>(retVal));
}

void CodegenVisitor::visit(const YieldStmt *stmt) {
  ctx->getBase()->setGenerator();

  auto dst = newBlock();
  auto yieldVal = stmt->expr ? toOperand(transform(stmt->expr)) : nullptr;
  ctx->getBlock()->setTerminator(
      make_shared<YieldTerminator>(dst, yieldVal, weak_ptr<ir::Var>()));
  ctx->replaceBlock(dst);
}

void CodegenVisitor::visit(const AssertStmt *stmt) {
  auto dst = newBlock();
  auto op = toOperand(transform(stmt->expr));
  ctx->getBlock()->setTerminator(make_shared<AssertTerminator>(op, dst));
  ctx->replaceBlock(dst);
}

void CodegenVisitor::visit(const WhileStmt *stmt) {
  auto cond = newBlock();
  auto begin = newBlock();
  auto end = newBlock();
  begin->setAttribute(kLoopAttribute,
                      make_shared<LoopAttribute>(weak_ptr<BasicBlock>(), cond, begin,
                                                 weak_ptr<BasicBlock>(), end));

  ctx->addBlock(cond);
  auto condOp = toOperand(transform(stmt->cond));
  ctx->getBlock()->setTerminator(make_shared<CondJumpTerminator>(begin, end, condOp));
  ctx->popBlock();

  ctx->addBlock(begin);
  transform(stmt->suite);
  condSetTerminator(make_shared<JumpTerminator>(cond));
  ctx->popBlock();

  ctx->getBlock()->setTerminator(make_shared<JumpTerminator>(cond));

  ctx->replaceBlock(end);
}

void CodegenVisitor::visit(const ForStmt *stmt) {
  auto setup = newBlock();
  auto cond = newBlock();
  auto begin = newBlock();
  auto end = newBlock();
  begin->setAttribute(
      kLoopAttribute,
      make_shared<LoopAttribute>(setup, cond, begin, weak_ptr<BasicBlock>(), end));

  auto doneVar = make_shared<ir::Var>(ir::types::kBoolType);
  ctx->getBase()->addVar(doneVar);

  auto varId = CAST(stmt->var, IdExpr);
  auto resVar =
      make_shared<ir::Var>(varId->value, realizeType(varId->getType()->getClass()));
  ctx->addVar(varId->value, resVar);
  ctx->getBase()->addVar(resVar);

  ctx->addBlock(setup);
  auto doneSetupRval = toRvalue(transform(stmt->done));
  ctx->getBlock()->add(
      make_shared<AssignInstr>(make_shared<VarLvalue>(doneVar), doneSetupRval));
  ctx->getBlock()->setTerminator(make_shared<JumpTerminator>(cond));
  ctx->popBlock();

  ctx->addBlock(cond);
  ctx->getBlock()->setTerminator(
      make_shared<CondJumpTerminator>(end, begin, make_shared<VarOperand>(doneVar)));
  ctx->popBlock();

  ctx->addBlock(begin);
  auto nextRval = toRvalue(transform(stmt->next));
  ctx->getBlock()->add(
      make_shared<AssignInstr>(make_shared<VarLvalue>(resVar), nextRval));
  transform(stmt->suite);
  auto doneRval = toRvalue(transform(stmt->done));
  ctx->getBlock()->add(
      make_shared<AssignInstr>(make_shared<VarLvalue>(doneVar), doneRval));
  condSetTerminator(make_shared<JumpTerminator>(cond));
  ctx->popBlock();

  ctx->getBlock()->setTerminator(make_shared<JumpTerminator>(setup));

  ctx->replaceBlock(end);
}

void CodegenVisitor::visit(const IfStmt *stmt) {
  auto firstCond = newBlock();
  auto end = newBlock();
  auto check = firstCond;

  bool elseEncountered = false;

  for (auto &i : stmt->ifs) {
    if (i.cond) {
      auto newCheck = newBlock();
      auto body = newBlock();

      ctx->addBlock(check);
      auto cond = toOperand(transform(i.cond));
      ctx->getBlock()->setTerminator(
          make_shared<CondJumpTerminator>(body, newCheck, cond));
      ctx->popBlock();

      ctx->addBlock(body);
      transform(i.suite);
      condSetTerminator(make_shared<JumpTerminator>(end));
      ctx->popBlock();

      check = newCheck;
    } else {
      elseEncountered = true;

      auto body = check;
      ctx->addBlock(body);
      transform(i.suite);
      condSetTerminator(make_shared<JumpTerminator>(end));
      ctx->popBlock();
    }
  }

  if (!elseEncountered) {
    ctx->addBlock(check);
    ctx->getBlock()->setTerminator(make_shared<JumpTerminator>(end));
    ctx->popBlock();
  }

  ctx->getBlock()->setTerminator(make_shared<JumpTerminator>(firstCond));
  ctx->replaceBlock(end);
}

void CodegenVisitor::visit(const MatchStmt *stmt) {
  auto firstCond = newBlock();
  auto end = newBlock();
  auto check = firstCond;

  for (auto ci = 0; ci < stmt->cases.size(); ci++) {
    string varName;
    shared_ptr<ir::Var> var;
    shared_ptr<ir::Pattern> pat;

    auto newCheck = newBlock();
    auto body = newBlock();

    ctx->addBlock(check);
    if (auto p = CAST(stmt->patterns[ci], BoundPattern)) {
      ctx->addBlock(check);
      auto boundPat =
          make_shared<ir::BoundPattern>(transform(p->pattern).patternResult);
      var = boundPat->getVar();
      varName = p->var;
      pat = std::static_pointer_cast<ir::Pattern>(boundPat);
    } else {
      pat = transform(stmt->patterns[ci]).patternResult;
    }

    auto t = make_shared<ir::Var>(ir::types::kBoolType);
    ctx->getBase()->addVar(t);
    auto matchOp = toOperand(transform(stmt->what));
    ctx->getBlock()->add(make_shared<AssignInstr>(
        make_shared<VarLvalue>(t), make_shared<MatchRvalue>(pat, matchOp)));
    ctx->getBlock()->setTerminator(
        make_shared<CondJumpTerminator>(body, newCheck, make_shared<VarOperand>(t)));
    ctx->popBlock();

    if (var)
      ctx->addVar(varName, var);

    ctx->addBlock(body);
    transform(stmt->cases[ci]);
    condSetTerminator(make_shared<JumpTerminator>(end));
    ctx->popBlock();

    check = newCheck;
  }

  ctx->addBlock(check);
  ctx->getBlock()->setTerminator(make_shared<JumpTerminator>(end));
  ctx->popBlock();

  ctx->getBlock()->setTerminator(make_shared<JumpTerminator>(firstCond));

  ctx->replaceBlock(end);
}

void CodegenVisitor::visit(const TryStmt *stmt) {
  auto parentTc = ctx->getBlock()->getAttribute(kTryCatchAttribute)
                      ? std::static_pointer_cast<TryCatchAttribute>(
                            ctx->getBlock()->getAttribute(kTryCatchAttribute))
                            ->handler
                      : ctx->getModule()->getTryCatch();
  auto newTc = make_shared<ir::TryCatch>();
  if (parentTc)
    parentTc->addChild(newTc);

  auto end = newBlock();
  auto body = newBlock();
  body->setAttribute(kTryCatchAttribute, make_shared<TryCatchAttribute>(newTc));

  ctx->addBlock(body);
  transform(stmt->suite);
  condSetTerminator(make_shared<JumpTerminator>(end));
  ctx->popBlock();

  int varIdx = 0;
  for (auto &c : stmt->catches) {
    /// TODO: get rid of typeinfo here?
    auto cBlock = newBlock();
    ctx->addBlock(cBlock);
    transform(c.suite);
    condSetTerminator(make_shared<JumpTerminator>(end));
    ctx->popBlock();

    newTc->addCatch(c.exc->getType() ? realizeType(c.exc->getType()->getClass())
                                     : nullptr,
                    c.var, newBlock());
    ctx->addVar(c.var, newTc->getVar(varIdx));
  }
  if (stmt->finally) {
    auto fBlock = newBlock();
    ctx->addBlock(fBlock);
    transform(stmt->finally);
    ctx->popBlock();
    newTc->setFinallyBlock(fBlock);
  }
}

void CodegenVisitor::visit(const ThrowStmt *stmt) {
  auto op = toOperand(transform(stmt->expr));
  ctx->getBlock()->setTerminator(make_shared<ThrowTerminator>(op));
}

void CodegenVisitor::visit(const FunctionStmt *stmt) {
  for (auto &real : ctx->cache->realizations[stmt->name]) {
    auto &fp = ctx->functions[real.first];
    if (fp.second)
      continue;

    fp.second = true;
    auto f = fp.first;
    assert(f);
    auto ast = (FunctionStmt *)(ctx->cache->realizationAsts[real.first].get());
    assert(ast);

    LOG7("[codegen] generating fn {}", real.first);
    // f->setName(real.first);
    f->setAttribute(kSrcInfoAttribute, make_shared<SrcInfoAttribute>(getSrcInfo()));
    if (!ctx->isToplevel())
      f->setEnclosingFunc(ctx->getBase());
    ctx->addBlock(f->getBlocks()[0], f);
    vector<string> names;
    vector<std::shared_ptr<ir::types::Type>> types;

    auto t = real.second->getFunc();
    for (int i = 1; i < t->args.size(); i++) {
      types.push_back(realizeType(t->args[i]->getClass()));
      names.push_back(ast->args[i - 1].name);
    }
    f->setArgNames(names);

    f->setType(realizeType(t->getClass()));
    f->setAttribute(kFuncAttribute, make_shared<FuncAttribute>(ast->attributes));
    for (auto a : ast->attributes) {
      if (a == "atomic")
        ctx->setFlag("atomic");
    }
    if (in(ast->attributes, ".c")) {
      auto newName = ctx->cache->reverseLookup[stmt->name];
      f->setName(newName);
      f->setExternal();
    } else if (!in(ast->attributes, "internal")) {
      for (auto &arg : names)
        ctx->addVar(arg, f->getArgVar(arg));
      transform(ast->suite);
    }
    condSetTerminator(make_shared<ReturnTerminator>(nullptr));
    ctx->popBlock();
  }
}

void CodegenVisitor::visitMethods(const string &name) {
  // auto c = ctx->getRealizations()->findClass(name);
  // if (c)
  //   for (auto &m : c->methods)
  //     for (auto &mm : m.second) {
  //       FunctionStmt *f = CAST(ctx->getRealizations()->getAST(mm->name),
  //       FunctionStmt); visit(f);
  //     }
}

void CodegenVisitor::visit(const ClassStmt *stmt) {
  // visitMethods(ctx->getRealizations()->getCanonicalName(stmt->getSrcInfo()));
}

void CodegenVisitor::visit(const StarPattern *pat) {
  result = CodegenResult(make_shared<ir::StarPattern>());
}

void CodegenVisitor::visit(const IntPattern *pat) {
  result = CodegenResult(make_shared<ir::IntPattern>(pat->value));
}

void CodegenVisitor::visit(const BoolPattern *pat) {
  result = CodegenResult(make_shared<ir::BoolPattern>(pat->value));
}

void CodegenVisitor::visit(const StrPattern *pat) {
  result = CodegenResult(make_shared<ir::StrPattern>(pat->value));
}

void CodegenVisitor::visit(const SeqPattern *pat) {
  result = CodegenResult(make_shared<ir::SeqPattern>(pat->value));
}

void CodegenVisitor::visit(const RangePattern *pat) {
  result = CodegenResult(make_shared<ir::RangePattern>(pat->start, pat->end));
}

void CodegenVisitor::visit(const TuplePattern *pat) {
  vector<shared_ptr<ir::Pattern>> pp;
  for (auto &p : pat->patterns)
    pp.push_back(transform(p).patternResult);
  result = CodegenResult(make_shared<ir::RecordPattern>(move(pp)));
}

void CodegenVisitor::visit(const ListPattern *pat) {
  vector<shared_ptr<ir::Pattern>> pp;
  for (auto &p : pat->patterns)
    pp.push_back(transform(p).patternResult);
  result = CodegenResult(make_shared<ir::ArrayPattern>(move(pp)));
}

void CodegenVisitor::visit(const OrPattern *pat) {
  vector<shared_ptr<ir::Pattern>> pp;
  for (auto &p : pat->patterns)
    pp.push_back(transform(p).patternResult);
  result = CodegenResult(make_shared<ir::OrPattern>(move(pp)));
}

void CodegenVisitor::visit(const WildcardPattern *pat) {
  auto p = make_shared<ir::WildcardPattern>();
  ctx->addVar(pat->var, p->getVar());
  result = CodegenResult(p);
}

void CodegenVisitor::visit(const GuardedPattern *pat) {
  result = CodegenResult(make_shared<ir::GuardedPattern>(
      transform(pat->pattern).patternResult, toOperand(transform(pat->cond))));
}

std::shared_ptr<ir::types::Type> CodegenVisitor::realizeType(types::ClassTypePtr t) {
  auto i = ctx->types.find(t->getClass()->realizeString());
  assert(i != ctx->types.end());
  return i->second;
}

} // namespace ast
} // namespace seq
