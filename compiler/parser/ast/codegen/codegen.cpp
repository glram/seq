#include "util/fmt/format.h"
#include "util/fmt/ostream.h"
#include <memory>
#include <ostream>
#include <stack>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "sir/base.h"
#include "sir/instr.h"
#include "sir/terminator.h"
#include "sir/trycatch.h"

#include "parser/ast/ast.h"
#include "parser/ast/codegen/codegen.h"
#include "parser/ast/codegen/codegen_ctx.h"
#include "parser/common.h"

using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
using std::weak_ptr;

using namespace seq::ir;

namespace {
string temporaryName(const string &name) { return "._" + name; }
} // namespace

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
  auto templ = ctx->getBlock();
  auto ret = make_shared<BasicBlock>(templ->getTryCatch(), templ->isCatchClause());

  ctx->getBase()->addBlock(ret);

  if (templ->getAttribute(kLoopAttribute))
    ret->setAttribute(kLoopAttribute,
                      make_shared<LoopAttribute>(
                          *templ->getAttribute<LoopAttribute>(kLoopAttribute)));

  return ret;
}

void CodegenVisitor::condSetTerminator(shared_ptr<Terminator> term) {
  if (!ctx->getBlock()->getTerminator())
    ctx->getBlock()->setTerminator(std::move(term));
}

shared_ptr<Operand> CodegenVisitor::toOperand(CodegenResult res) {
  switch (res.tag) {
  case CodegenResult::OP:
    return res.operandResult;
  case CodegenResult::LVALUE:
    seqassert(false, "cannot convert lvalue to operand.");
    return nullptr;
  case CodegenResult::RVALUE: {
    auto srcInfoAttr =
        res.rvalueResult->getAttribute<SrcInfoAttribute>(kSrcInfoAttribute);
    auto srcInfo = srcInfoAttr ? srcInfoAttr->info : seq::SrcInfo();
    auto t = Ns<ir::Var>(srcInfo, res.typeOverride ? res.typeOverride
                                                   : res.rvalueResult->getType());
    ctx->getBase()->addVar(t);
    ctx->getBlock()->add(
        Ns<AssignInstr>(srcInfo, Ns<VarLvalue>(srcInfo, t), res.rvalueResult));
    return Ns<VarOperand>(srcInfo, t);
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

shared_ptr<Rvalue> CodegenVisitor::toRvalue(CodegenResult res) {
  switch (res.tag) {
  case CodegenResult::OP: {
    auto srcInfoAttr =
        res.operandResult->getAttribute<SrcInfoAttribute>(kSrcInfoAttribute);
    auto srcInfo = srcInfoAttr ? srcInfoAttr->info : seq::SrcInfo();
    return Ns<OperandRvalue>(srcInfo, res.operandResult);
  }
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
  expr->accept(v);
  return v.result;
}

CodegenResult CodegenVisitor::transform(const StmtPtr &stmt) {
  CodegenVisitor v(ctx);
  stmt->accept(v);
  return v.result;
}

CodegenResult CodegenVisitor::transform(const PatternPtr &ptr) {
  CodegenVisitor v(ctx);
  ptr->accept(v);
  return v.result;
}

shared_ptr<IRModule> CodegenVisitor::apply(shared_ptr<Cache> cache, StmtPtr stmts) {
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
          auto p = t->parent;
          assert(in(ast->attributes, ".class"));
          if (!in(ast->attributes, ".method")) { // hack for non-generic types
            for (auto &x : ctx->cache->realizations[ast->attributes[".class"]]) {
              if (startswith(t->realizeString(), x.first)) {
                p = x.second;
                break;
              }
            }
          }
          seqassert(p && p->getClass(), "parent must be set ({}) for {}; parent={}",
                    p ? p->toString() : "-", t->toString(), ast->attributes[".class"]);
          shared_ptr<ir::types::Type> typ = ctx->realizeType(p->getClass());
          int startI = 1;
          if (ast->args.size() && ast->args[0].name == "self")
            startI = 2;
          for (int i = startI; i < t->args.size(); i++)
            types.push_back(ctx->realizeType(t->args[i]->getClass()));
          if (isdigit(name[0])) // TODO: get rid of this hack
            name = names[names.size() - 2];
          LOG7("[codegen] generating internal fn {} -> {}", ast->name, name);
          auto fn = Ns<ir::Func>(t->getSrcInfo(), names.back(), vector<string>(),
                                 ir::types::kNoArgVoidFuncType);
          fn->setInternal(typ, name);
          ctx->functions[f.first] = {fn, false};
          ctx->getModule()->addGlobal(fn);
        } else {
          auto fn = Ns<ir::Func>(t->getSrcInfo(), name, vector<string>(),
                                 ir::types::kNoArgVoidFuncType);
          ctx->functions[f.first] = {fn, false};
          ctx->getModule()->addGlobal(fn);

          if (in(ast->attributes, "builtin")) {
            fn->setBuiltin(name);
          }
        }
        ctx->addFunc(f.first, ctx->functions[f.first].first);
      }
  CodegenVisitor(ctx).transform(stmts);
  if (!ctx->getBlock()->getTerminator())
    ctx->getBlock()->setTerminator(Ns<ReturnTerminator>({}, nullptr));
  return module;
}

void CodegenVisitor::visit(const BoolExpr *expr) {
  result = CodegenResult(Ns<LiteralOperand>(expr->getSrcInfo(), expr->value));
}

void CodegenVisitor::visit(const IntExpr *expr) {
  if (!expr->sign)
    result = CodegenResult(
        Ns<LiteralOperand>(expr->getSrcInfo(), int64_t(uint64_t(expr->intValue))));
  else
    result = CodegenResult(Ns<LiteralOperand>(expr->getSrcInfo(), expr->intValue));
}

void CodegenVisitor::visit(const FloatExpr *expr) {
  result = CodegenResult(Ns<LiteralOperand>(expr->getSrcInfo(), expr->value));
}

void CodegenVisitor::visit(const StringExpr *expr) {
  result = CodegenResult(Ns<LiteralOperand>(expr->getSrcInfo(), expr->value));
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
    result = CodegenResult(Ns<VarOperand>(expr->getSrcInfo(), v));
  else if (auto f = val->getFunc())
    result = CodegenResult(Ns<VarOperand>(expr->getSrcInfo(), f));
  else
    result = CodegenResult(val->getType());
}

void CodegenVisitor::visit(const IfExpr *expr) {
  auto var = Ns<ir::Var>(expr->getSrcInfo(), temporaryName("if_res"),
                         realizeType(expr->getType()->getClass()));
  ctx->getBase()->addVar(var);

  auto tBlock = newBlock();
  auto fBlock = newBlock();
  auto nextBlock = newBlock();

  auto condResultOp = toOperand(transform(expr->cond));
  ctx->getBlock()->setTerminator(
      Ns<CondJumpTerminator>(expr->getSrcInfo(), tBlock, fBlock, condResultOp));

  ctx->addBlock(tBlock);
  auto tResultOp = toOperand(transform(expr->eif));
  ctx->getBlock()->add(
      Ns<AssignInstr>(expr->getSrcInfo(), Ns<VarLvalue>(expr->getSrcInfo(), var),
                      Ns<OperandRvalue>(expr->getSrcInfo(), tResultOp)));
  ctx->getBlock()->setTerminator(Ns<JumpTerminator>(expr->getSrcInfo(), nextBlock));
  ctx->popBlock();

  ctx->addBlock(fBlock);
  auto fResultOp = toOperand(transform(expr->eelse));
  ctx->getBlock()->add(
      Ns<AssignInstr>(expr->getSrcInfo(), Ns<VarLvalue>(expr->getSrcInfo(), var),
                      Ns<OperandRvalue>(expr->getSrcInfo(), fResultOp)));
  ctx->getBlock()->setTerminator(Ns<JumpTerminator>(expr->getSrcInfo(), nextBlock));
  ctx->popBlock();

  ctx->replaceBlock(nextBlock);

  result = CodegenResult(Ns<VarOperand>(expr->getSrcInfo(), var));
}

void CodegenVisitor::visit(const BinaryExpr *expr) {
  assert(expr->op == "&&" || expr->op == "||");

  auto var =
      Ns<ir::Var>(expr->getSrcInfo(), temporaryName("bin_res"), ir::types::kBoolType);
  ctx->getBase()->addVar(var);

  auto trueBlock = newBlock();
  auto falseBlock = newBlock();
  auto nextBlock = newBlock();

  auto lhsOp = toOperand(transform(expr->lexpr));
  if (expr->op == "&&") {
    auto tLeftBlock = newBlock();
    ctx->getBlock()->setTerminator(
        Ns<CondJumpTerminator>(expr->getSrcInfo(), tLeftBlock, falseBlock, lhsOp));

    ctx->addBlock(tLeftBlock);
    auto rhsOp = toOperand(transform(expr->rexpr));
    ctx->getBlock()->setTerminator(
        Ns<CondJumpTerminator>(expr->getSrcInfo(), trueBlock, falseBlock, rhsOp));
    ctx->popBlock();
  } else {
    auto fLeftBlock = newBlock();
    ctx->getBlock()->setTerminator(
        Ns<CondJumpTerminator>(expr->getSrcInfo(), trueBlock, fLeftBlock, lhsOp));

    ctx->addBlock(fLeftBlock);
    auto rhsOp = toOperand(transform(expr->rexpr));
    ctx->getBlock()->setTerminator(
        Ns<CondJumpTerminator>(expr->getSrcInfo(), trueBlock, falseBlock, rhsOp));
    ctx->popBlock();
  }

  ctx->addBlock(trueBlock);
  auto trueOperand = Ns<LiteralOperand>(expr->getSrcInfo(), true);
  ctx->getBlock()->add(
      Ns<AssignInstr>(expr->getSrcInfo(), Ns<VarLvalue>(expr->getSrcInfo(), var),
                      Ns<OperandRvalue>(expr->getSrcInfo(), trueOperand)));
  ctx->getBlock()->setTerminator(Ns<JumpTerminator>(expr->getSrcInfo(), nextBlock));
  ctx->popBlock();

  ctx->addBlock(falseBlock);
  auto falseOperand = Ns<LiteralOperand>(expr->getSrcInfo(), false);
  ctx->getBlock()->add(
      Ns<AssignInstr>(expr->getSrcInfo(), Ns<VarLvalue>(expr->getSrcInfo(), var),
                      Ns<OperandRvalue>(expr->getSrcInfo(), falseOperand)));
  ctx->getBlock()->setTerminator(Ns<JumpTerminator>(expr->getSrcInfo(), nextBlock));
  ctx->popBlock();

  ctx->replaceBlock(nextBlock);

  result = CodegenResult(Ns<VarOperand>(expr->getSrcInfo(), var));
}

void CodegenVisitor::visit(const PipeExpr *expr) {
  //<<<<<<< HEAD
  //  vector<shared_ptr<Operand>> ops;
  //  vector<bool> parallel;
  //  for (const auto &item : expr->items) {
  //    ops.push_back(toOperand(transform(item.expr)));
  //    parallel.push_back(item.op == "||>");
  //=======
  //  vector<seq::Expr *> exprs{transform(expr->items[0].expr)};
  //  vector<seq::types::Type *> inTypes{realizeType(expr->inTypes[0]->getClass())};
  //  for (int i = 1; i < expr->items.size(); i++) {
  //    auto e = CAST(expr->items[i].expr, CallExpr);
  //    assert(e);
  //    // LOG("{}", e->toString());
  //
  //    auto pfn = transform(e->expr);
  //    // LOG(" -- {} ... {}", pfn->getType()->getName(), e->args.size());
  //    vector<seq::Expr *> items(e->args.size(), nullptr);
  //    vector<string> names(e->args.size(), "");
  //    vector<seq::types::Type *> partials(e->args.size(), nullptr);
  //    for (int ai = 0; ai < e->args.size(); ai++)
  //      if (!CAST(e->args[ai].value, EllipsisExpr)) {
  //        items[ai] = transform(e->args[ai].value);
  //        partials[ai] = realizeType(e->args[ai].value->getType()->getClass());
  //        // LOG(" -- {}: {} .. {}", ai, partials[ai]->getName(),
  //        // items[ai]->getType()->getName());
  //      }
  //    auto p = new seq::PartialCallExpr(pfn, items, names);
  //    p->setType(seq::types::PartialFuncType::get(pfn->getType(), partials));
  //    // LOG(" ?? {}", p->getType()->getName())
  //
  //    exprs.push_back(p);
  //    inTypes.push_back(realizeType(expr->inTypes[i]->getClass()));
  //>>>>>>> af86e35772e7039640dd5ff30e1edcfd12d9eca1
  //  }
  //  result = CodegenResult(Ns<PipelineRvalue>(expr->getSrcInfo(), ops, parallel));

  vector<shared_ptr<Operand>> ops = {toOperand(transform(expr->items[0].expr))};
  vector<bool> parallel = {expr->items[0].op == "||>"};
  vector<shared_ptr<ir::types::Type>> inTypes = {
      realizeType(expr->inTypes[0]->getClass())};
  for (auto i = 1; i < expr->items.size(); ++i) {
    auto e = CAST(expr->items[i].expr, CallExpr);
    assert(e);

    auto pfn = toOperand(transform(e->expr));
    vector<shared_ptr<Operand>> items(e->args.size(), nullptr);
    vector<string> names(e->args.size(), "");
    vector<shared_ptr<ir::types::Type>> partials(e->args.size(), nullptr);

    for (int ai = 0; ai < e->args.size(); ai++)
      if (!CAST(e->args[ai].value, EllipsisExpr)) {
        items[ai] = toOperand(transform(e->args[ai].value));
        partials[ai] = realizeType(e->args[ai].value->getType()->getClass());
      }
    auto pType = std::static_pointer_cast<ir::types::PartialFuncType>(
        realizeType(e->getType()->getClass()));
    auto tVar = Ns<ir::Var>(expr->getSrcInfo(), pType);
    ctx->getBase()->addVar(tVar);
    ctx->getBlock()->add(
        Ns<AssignInstr>(expr->getSrcInfo(), Ns<VarLvalue>(expr->getSrcInfo(), tVar),
                        Ns<PartialCallRvalue>(expr->getSrcInfo(), pfn, items, pType)));
    ops.push_back(Ns<VarOperand>(expr->getSrcInfo(), tVar));
    inTypes.push_back(realizeType(expr->inTypes[i]->getClass()));
  }
  result = CodegenResult(Ns<PipelineRvalue>(expr->getSrcInfo(), ops, parallel, inTypes,
                                            realizeType(expr->getType()->getClass())));
}

void CodegenVisitor::visit(const CallExpr *expr) {
  auto lhs = toOperand(transform(expr->expr));
  vector<shared_ptr<Operand>> items;
  for (auto &&i : expr->args) {
    if (CAST(i.value, EllipsisExpr)) {
      assert(false);
    } else {
      items.push_back(toOperand(transform(i.value)));
    }
  }
  result = CodegenResult(Ns<CallRvalue>(expr->getSrcInfo(), lhs, items));
  result.typeOverride = realizeType(expr->getType()->getClass());
}

void CodegenVisitor::visit(const StackAllocExpr *expr) {
  auto arrayType = std::static_pointer_cast<ir::types::Array>(
      realizeType(expr->getType()->getClass()));
  auto e = CAST(expr->expr, IntExpr);
  result =
      CodegenResult(Ns<StackAllocRvalue>(expr->getSrcInfo(), arrayType, e->intValue));
}

void CodegenVisitor::visit(const DotExpr *expr) {
  result = CodegenResult(Ns<MemberRvalue>(
      expr->getSrcInfo(), toOperand(transform(expr->expr)), expr->member));
}

void CodegenVisitor::visit(const PtrExpr *expr) {
  auto e = CAST(expr->expr, IdExpr);
  assert(e);
  auto v = ctx->find(e->value, true)->getVar();
  assert(v);
  result = CodegenResult(Ns<VarPointerOperand>(
      expr->getSrcInfo(), realizeType(expr->getType()->getClass()), v));
}

void CodegenVisitor::visit(const YieldExpr *expr) {
  ctx->getBase()->setGenerator();

  auto var = Ns<ir::Var>(expr->getSrcInfo(), temporaryName("yield_res"),
                         realizeType(expr->getType()->getClass()));
  ctx->getBase()->addVar(var);

  auto dst = newBlock();
  ctx->getBlock()->setTerminator(
      Ns<YieldTerminator>(expr->getSrcInfo(), dst, nullptr, var));
  ctx->replaceBlock(dst);

  result = CodegenResult(Ns<VarOperand>(expr->getSrcInfo(), var));
}

void CodegenVisitor::visit(const StmtExpr *expr) {
  for (auto &s : expr->stmts) {
    transform(s);
  }
  result = transform(expr->expr);
}

void CodegenVisitor::visit(const SuiteStmt *stmt) {
  for (auto &s : stmt->stmts) {
    transform(s);
  }
}

void CodegenVisitor::visit(const PassStmt *stmt) {}

void CodegenVisitor::visit(const BreakStmt *stmt) {
  auto loop = ctx->getBlock()->getAttribute<LoopAttribute>(kLoopAttribute);
  auto dst = loop->end.lock();
  if (!dst)
    seqassert(false, "No loop end");
  ctx->getBlock()->setTerminator(Ns<JumpTerminator>(stmt->getSrcInfo(), dst));
}

void CodegenVisitor::visit(const ContinueStmt *stmt) {
  auto loop = ctx->getBlock()->getAttribute<LoopAttribute>(kLoopAttribute);
  auto dst = loop->cond.lock();
  if (!dst)
    dst = loop->begin.lock();
  if (!dst)
    seqassert(false, "No loop body");
  ctx->getBlock()->setTerminator(Ns<JumpTerminator>(stmt->getSrcInfo(), dst));
}

void CodegenVisitor::visit(const ExprStmt *stmt) {
  auto rval = toRvalue(transform(stmt->expr));
  ctx->getBlock()->add(Ns<RvalueInstr>(stmt->getSrcInfo(), rval));
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
    auto v = Ns<ir::Var>(stmt->getSrcInfo(), var,
                         realizeType(stmt->rhs->getType()->getClass()));
    if (var[0] == '.')
      ctx->getModule()->addGlobal(v);
    else
      ctx->getBase()->addVar(v);
    ctx->addVar(var, v);
    auto val = ctx->find(var);
    auto rval = toRvalue(transform(stmt->rhs));
    ctx->getBlock()->add(Ns<AssignInstr>(stmt->getSrcInfo(),
                                         Ns<VarLvalue>(stmt->getSrcInfo(), v), rval));
  }
}

void CodegenVisitor::visit(const AssignMemberStmt *stmt) {
  auto i = CAST(stmt->lhs, IdExpr);
  auto name = i->value;

  auto var = ctx->find(name, false);
  auto rhs = toRvalue(transform(stmt->rhs));
  ctx->getBlock()->add(Ns<AssignInstr>(
      stmt->getSrcInfo(),
      Ns<VarMemberLvalue>(stmt->getSrcInfo(), var->getVar(), stmt->member), rhs));
}

void CodegenVisitor::visit(const UpdateStmt *stmt) {
  auto i = CAST(stmt->lhs, IdExpr);
  assert(i);
  auto var = i->value;
  auto val = ctx->find(var, true);
  assert(val && val->getVar());
  auto rhs = toRvalue(transform(stmt->rhs));
  ctx->getBlock()->add(Ns<AssignInstr>(
      stmt->getSrcInfo(), Ns<VarLvalue>(stmt->getSrcInfo(), val->getVar()), rhs));
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
  ctx->getBlock()->setTerminator(Ns<ReturnTerminator>(stmt->getSrcInfo(), retVal));
}

void CodegenVisitor::visit(const YieldStmt *stmt) {
  ctx->getBase()->setGenerator();

  auto dst = newBlock();
  auto yieldVal = stmt->expr ? toOperand(transform(stmt->expr)) : nullptr;
  ctx->getBlock()->setTerminator(
      Ns<YieldTerminator>(stmt->getSrcInfo(), dst, yieldVal, nullptr));
  ctx->replaceBlock(dst);
}

void CodegenVisitor::visit(const AssertStmt *stmt) {
  auto dst = newBlock();
  auto op = toOperand(transform(stmt->expr));
  ctx->getBlock()->setTerminator(Ns<AssertTerminator>(stmt->getSrcInfo(), op, dst));
  ctx->replaceBlock(dst);
}

void CodegenVisitor::visit(const WhileStmt *stmt) {
  auto cond = newBlock();
  auto begin = newBlock();
  auto end = newBlock();
  begin->setAttribute(kLoopAttribute,
                      make_shared<LoopAttribute>(weak_ptr<BasicBlock>(), cond, begin,
                                                 weak_ptr<BasicBlock>(), end));

  ctx->addLevel();

  ctx->addBlock(cond);
  auto condOp = toOperand(transform(stmt->cond));
  ctx->getBlock()->setTerminator(
      Ns<CondJumpTerminator>(stmt->getSrcInfo(), begin, end, condOp));
  ctx->popBlock();

  ctx->addBlock(begin);
  transform(stmt->suite);
  condSetTerminator(Ns<JumpTerminator>(stmt->getSrcInfo(), cond));
  ctx->popBlock();

  ctx->getBlock()->setTerminator(Ns<JumpTerminator>(stmt->getSrcInfo(), cond));

  ctx->removeLevel();
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

  ctx->addLevel();
  transform(stmt->iter);

  auto doneVar =
      Ns<ir::Var>(stmt->getSrcInfo(), temporaryName("for_done"), ir::types::kBoolType);
  ctx->getBase()->addVar(doneVar);

  auto varId = CAST(stmt->var, IdExpr);
  auto resVar = Ns<ir::Var>(stmt->getSrcInfo(), varId->value,
                            realizeType(varId->getType()->getClass()));
  ctx->addVar(varId->value, resVar);
  ctx->getBase()->addVar(resVar);

  ctx->addBlock(setup);
  auto doneSetupRval = toRvalue(transform(stmt->done));
  ctx->getBlock()->add(Ns<AssignInstr>(
      stmt->getSrcInfo(), Ns<VarLvalue>(stmt->getSrcInfo(), doneVar), doneSetupRval));
  ctx->getBlock()->setTerminator(Ns<JumpTerminator>(stmt->getSrcInfo(), cond));
  ctx->popBlock();

  ctx->addBlock(cond);
  ctx->getBlock()->setTerminator(Ns<CondJumpTerminator>(
      stmt->getSrcInfo(), end, begin, Ns<VarOperand>(stmt->getSrcInfo(), doneVar)));
  ctx->popBlock();

  ctx->addBlock(begin);
  auto nextRval = toRvalue(transform(stmt->next));
  ctx->getBlock()->add(Ns<AssignInstr>(
      stmt->getSrcInfo(), Ns<VarLvalue>(stmt->getSrcInfo(), resVar), nextRval));
  transform(stmt->suite);

  auto doneRval = toRvalue(transform(stmt->done));
  ctx->getBlock()->add(Ns<AssignInstr>(
      stmt->getSrcInfo(), Ns<VarLvalue>(stmt->getSrcInfo(), doneVar), doneRval));
  condSetTerminator(Ns<JumpTerminator>(stmt->getSrcInfo(), cond));
  ctx->popBlock();

  ctx->getBlock()->setTerminator(Ns<JumpTerminator>(stmt->getSrcInfo(), setup));

  ctx->removeLevel();
  ctx->replaceBlock(end);
}

void CodegenVisitor::visit(const IfStmt *stmt) {
  auto firstCond = newBlock();
  auto end = newBlock();
  auto check = firstCond;

  bool elseEncountered = false;

  for (auto &i : stmt->ifs) {
    ctx->addLevel();
    if (i.cond) {
      auto newCheck = newBlock();
      auto body = newBlock();

      ctx->addBlock(check);
      auto cond = toOperand(transform(i.cond));
      ctx->getBlock()->setTerminator(
          Ns<CondJumpTerminator>(stmt->getSrcInfo(), body, newCheck, cond));
      ctx->popBlock();

      ctx->addBlock(body);
      transform(i.suite);
      condSetTerminator(Ns<JumpTerminator>(stmt->getSrcInfo(), end));
      ctx->popBlock();

      check = newCheck;
    } else {
      elseEncountered = true;

      auto body = check;
      ctx->addBlock(body);
      transform(i.suite);
      condSetTerminator(Ns<JumpTerminator>(stmt->getSrcInfo(), end));
      ctx->popBlock();
    }
    ctx->removeLevel();
  }

  if (!elseEncountered) {
    ctx->addBlock(check);
    ctx->getBlock()->setTerminator(Ns<JumpTerminator>(stmt->getSrcInfo(), end));
    ctx->popBlock();
  }

  ctx->getBlock()->setTerminator(Ns<JumpTerminator>(stmt->getSrcInfo(), firstCond));
  ctx->replaceBlock(end);
}

void CodegenVisitor::visit(const MatchStmt *stmt) {
  auto firstCond = newBlock();
  auto end = newBlock();
  auto check = firstCond;

  for (auto ci = 0; ci < stmt->cases.size(); ci++) {
    ctx->addLevel();

    string varName;
    shared_ptr<ir::Var> var;
    shared_ptr<ir::Pattern> pat;

    auto newCheck = newBlock();
    auto body = newBlock();

    ctx->addBlock(check);
    if (auto p = CAST(stmt->patterns[ci], BoundPattern)) {
      ctx->addBlock(check);
      auto boundPat =
          Ns<ir::BoundPattern>(stmt->getSrcInfo(), transform(p->pattern).patternResult,
                               realizeType(p->pattern->getType()->getClass()));
      var = boundPat->getVar();
      ctx->getBase()->addVar(var);
      varName = p->var;
      pat = std::static_pointer_cast<ir::Pattern>(boundPat);
    } else {
      pat = transform(stmt->patterns[ci]).patternResult;
    }

    auto t = Ns<ir::Var>(stmt->getSrcInfo(), temporaryName("match_res"),
                         ir::types::kBoolType);
    ctx->getBase()->addVar(t);
    auto matchOp = toOperand(transform(stmt->what));
    ctx->getBlock()->add(
        Ns<AssignInstr>(stmt->getSrcInfo(), Ns<VarLvalue>(stmt->getSrcInfo(), t),
                        Ns<MatchRvalue>(stmt->getSrcInfo(), pat, matchOp)));
    ctx->getBlock()->setTerminator(Ns<CondJumpTerminator>(
        stmt->getSrcInfo(), body, newCheck, Ns<VarOperand>(stmt->getSrcInfo(), t)));
    ctx->popBlock();

    if (var)
      ctx->addVar(varName, var);

    ctx->addBlock(body);
    transform(stmt->cases[ci]);
    condSetTerminator(Ns<JumpTerminator>(stmt->getSrcInfo(), end));
    ctx->popBlock();

    check = newCheck;
    ctx->removeLevel();
  }

  ctx->addBlock(check);
  ctx->getBlock()->setTerminator(Ns<JumpTerminator>(stmt->getSrcInfo(), end));
  ctx->popBlock();

  ctx->getBlock()->setTerminator(Ns<JumpTerminator>(stmt->getSrcInfo(), firstCond));

  ctx->replaceBlock(end);
}

void CodegenVisitor::visit(const TryStmt *stmt) {
  auto parentTc = ctx->getBlock()->getTryCatch() ? ctx->getBlock()->getTryCatch()
                                                 : ctx->getBase()->getTryCatch();
  auto newTc = Ns<ir::TryCatch>(stmt->getSrcInfo());

  if (parentTc)
    parentTc->addChild(newTc);
  else
    ctx->getBase()->setTryCatch(newTc);

  ctx->getBase()->addVar(newTc->getFlagVar());
  ctx->getBlock()->add(Ns<AssignInstr>(
      stmt->getSrcInfo(), Ns<VarLvalue>(stmt->getSrcInfo(), newTc->getFlagVar()),
      Ns<OperandRvalue>(stmt->getSrcInfo(),
                        Ns<LiteralOperand>(stmt->getSrcInfo(), int64_t(0)))));

  auto end = newBlock();
  auto body = newBlock();
  body->setTryCatch(newTc);
  ctx->getBlock()->setTerminator(Ns<JumpTerminator>(stmt->getSrcInfo(), body));

  ctx->addLevel();
  ctx->addBlock(body);
  transform(stmt->suite);
  condSetTerminator(Ns<JumpTerminator>(stmt->getSrcInfo(), end));
  ctx->popBlock();
  ctx->removeLevel();

  int varIdx = 0;
  for (auto &c : stmt->catches) {
    auto cBlock = newBlock();
    cBlock->setTryCatch(newTc, true);

    newTc->addCatch(c.exc ? realizeType(c.exc->getType()->getClass()) : nullptr, c.var,
                    cBlock);
    ctx->addLevel();
    if (!c.var.empty()) {
      ctx->addVar(c.var, newTc->getVar(varIdx));
      ctx->getBase()->addVar(newTc->getVar(varIdx));
    }
    ctx->addBlock(cBlock);
    transform(c.suite);
    condSetTerminator(Ns<JumpTerminator>(stmt->getSrcInfo(), end));
    ctx->popBlock();
    ctx->removeLevel();
  }
  if (stmt->finally) {
    ctx->addLevel();
    auto fBlock = newBlock();
    ctx->addBlock(fBlock);
    transform(stmt->finally);
    condSetTerminator(Ns<FinallyTerminator>(stmt->getSrcInfo(), newTc));
    ctx->popBlock();
    newTc->setFinallyBlock(fBlock);
    ctx->removeLevel();
  }
  ctx->replaceBlock(end);
}

void CodegenVisitor::visit(const ThrowStmt *stmt) {
  auto op = toOperand(transform(stmt->expr));
  ctx->getBlock()->setTerminator(Ns<ThrowTerminator>(stmt->getSrcInfo(), op));
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
    if (!ctx->isToplevel())
      f->setEnclosingFunc(ctx->getBase());
    ctx->addBlock(f->getBlocks()[0], f);
    vector<string> names;
    vector<shared_ptr<ir::types::Type>> types;

    auto t = real.second->getFunc();
    for (int i = 1; i < t->args.size(); i++) {
      types.push_back(realizeType(t->args[i]->getClass()));
      names.push_back(ast->args[i - 1].name);
    }
    f->setArgNames(names);
    f->setType(realizeType(t->getClass()));
    f->setAttribute(kFuncAttribute, make_shared<FuncAttribute>(ast->attributes));
    for (auto a : ast->attributes) {
      if (a.first == "atomic")

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
    condSetTerminator(Ns<ReturnTerminator>({}, nullptr));
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
  result = CodegenResult(Ns<ir::StarPattern>(pat->getSrcInfo()));
}

void CodegenVisitor::visit(const IntPattern *pat) {
  result = CodegenResult(Ns<ir::IntPattern>(pat->getSrcInfo(), pat->value));
}

void CodegenVisitor::visit(const BoolPattern *pat) {
  result = CodegenResult(Ns<ir::BoolPattern>(pat->getSrcInfo(), pat->value));
}

void CodegenVisitor::visit(const StrPattern *pat) {
  result = CodegenResult(Ns<ir::StrPattern>(pat->getSrcInfo(), pat->value));
}

void CodegenVisitor::visit(const SeqPattern *pat) {
  result = CodegenResult(Ns<ir::SeqPattern>(pat->getSrcInfo(), pat->value));
}

void CodegenVisitor::visit(const RangePattern *pat) {
  result = CodegenResult(Ns<ir::RangePattern>(pat->getSrcInfo(), pat->start, pat->end));
}

void CodegenVisitor::visit(const TuplePattern *pat) {
  vector<shared_ptr<ir::Pattern>> pp;
  for (auto &p : pat->patterns)
    pp.push_back(transform(p).patternResult);
  result = CodegenResult(Ns<ir::RecordPattern>(
      pat->getSrcInfo(), std::move(pp), realizeType(pat->getType()->getClass())));
}

void CodegenVisitor::visit(const ListPattern *pat) {
  vector<shared_ptr<ir::Pattern>> pp;
  for (auto &p : pat->patterns)
    pp.push_back(transform(p).patternResult);
  result = CodegenResult(Ns<ir::ArrayPattern>(pat->getSrcInfo(), std::move(pp),
                                              realizeType(pat->getType()->getClass())));
}

void CodegenVisitor::visit(const OrPattern *pat) {
  vector<shared_ptr<ir::Pattern>> pp;
  for (auto &p : pat->patterns)
    pp.push_back(transform(p).patternResult);
  result = CodegenResult(Ns<ir::OrPattern>(pat->getSrcInfo(), std::move(pp)));
}

void CodegenVisitor::visit(const WildcardPattern *pat) {
  auto p = Ns<ir::WildcardPattern>(pat->getSrcInfo(),
                                   realizeType(pat->getType()->getClass()));
  if (!pat->var.empty()) {
    ctx->addVar(pat->var, p->getVar());
  }
  ctx->getBase()->addVar(p->getVar());
  result = CodegenResult(p);
}

void CodegenVisitor::visit(const GuardedPattern *pat) {
  result = CodegenResult(Ns<ir::GuardedPattern>(pat->getSrcInfo(),
                                                transform(pat->pattern).patternResult,
                                                toOperand(transform(pat->cond))));
}

shared_ptr<ir::types::Type> CodegenVisitor::realizeType(types::ClassTypePtr t) {
  auto i = ctx->types.find(t->getClass()->realizeString());
  assert(i != ctx->types.end());
  return i->second;
}

} // namespace ast
} // namespace seq
