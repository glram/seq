#include "util/fmt/format.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "parser/ast/ast.h"
#include "parser/ast/codegen.h"
#include "parser/ast/codegen_ctx.h"
#include "parser/ast/format.h"
#include "parser/common.h"
#include "sir/base.h"
#include "sir/instr.h"
#include "sir/lvalue.h"
#include "sir/pattern.h"
#include "sir/rvalue.h"
#include "sir/terminator.h"
#include "sir/trycatch.h"
#include "sir/var.h"

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

using namespace seq::ir;

namespace seq {
namespace ast {

shared_ptr<BasicBlock> CodegenVisitor::newBlock() {
  auto ret = Ns<BasicBlock>();
  ctx->getBase()->addBlock(ret);

  auto templ = ctx->getBlock();
  if (templ->getAttribute(kLoopAttribute))
    ret->setAttribute(kLoopAttribute,
                      Ns<LoopAttribute>(*std::static_pointer_cast<LoopAttribute>(templ->getAttribute(kLoopAttribute))));
  if (templ->getAttribute(kTryCatchAttribute))
    ret->setAttribute(
        kTryCatchAttribute,
        Ns<TryCatchAttribute>(*std::static_pointer_cast<TryCatchAttribute>(templ->getAttribute(kTryCatchAttribute))));
  return ret;
}

void CodegenVisitor::condSetTerminator(shared_ptr<Terminator> term) {
  if (!ctx->getBlock()->getTerminator())
    ctx->getBlock()->setTerminator(std::move(term));
}

void CodegenVisitor::defaultVisit(const Expr *n) {
  internalError("invalid node {}", *n);
}

void CodegenVisitor::defaultVisit(const Stmt *n) {
  internalError("invalid node {}", *n);
}

void CodegenVisitor::defaultVisit(const Pattern *n) {
  internalError("invalid node {}", *n);
}

CodegenVisitor::CodegenVisitor(shared_ptr<LLVMContext> ctx)
    : ctx(ctx), result() {}

CodegenResult CodegenVisitor::transform(const Expr *expr) {
  if (!expr)
    return CodegenResult();
  CodegenVisitor v(ctx);
  v.setSrcInfo(expr->getSrcInfo());
  expr->accept(v);
  v.result.addAttribute(kSrcInfoAttribute,
                        std::make_shared<SrcInfoAttribute>(v.getSrcInfo()));
  return v.result;
}

std::shared_ptr<seq::ir::Operand>
CodegenVisitor::toOperand(const CodegenResult res) {
  switch (res.tag) {
  case CodegenResult::OP:
    return res.operandResult;
  case CodegenResult::LVALUE:
    internalError("cannot convert lvalue to operand.");
    return nullptr;
  case CodegenResult::RVALUE: {
    auto t = Ns<ir::Var>(res.rvalueResult->getType());
    ctx->getBase()->addVar(t);
    ctx->getBlock()->add(Ns<AssignInstr>(Ns<VarLvalue>(t), res.rvalueResult));
    return Ns<VarOperand>(t);
  }
  case CodegenResult::PATTERN:
    internalError("cannot convert pattern to operand.");
    return nullptr;
  default:
    internalError("cannot convert unknown to operand.");
    return nullptr;
  }
}

std::shared_ptr<seq::ir::Rvalue>
CodegenVisitor::toRvalue(const CodegenResult res) {
  switch (res.tag) {
  case CodegenResult::OP:
    return Ns<OperandRvalue>(res.operandResult);
  case CodegenResult::LVALUE:
    internalError("cannot convert lvalue to rvalue.");
    return nullptr;
  case CodegenResult::RVALUE:
    return res.rvalueResult;
  case CodegenResult::PATTERN:
    internalError("cannot convert pattern to rvalue.");
    return nullptr;
  default:
    internalError("cannot convert unknown to rvalue.");
    return nullptr;
  }
}

CodegenResult CodegenVisitor::transform(const Stmt *stmt) {
  CodegenVisitor v(ctx);
  stmt->accept(v);
  v.setSrcInfo(stmt->getSrcInfo());
  v.result.addAttribute(kSrcInfoAttribute,
                        std::make_shared<SrcInfoAttribute>(v.getSrcInfo()));
  //    v.resultStmt->setBase(ctx->getBase());
  //    ctx->getBlock()->add(v.resultStmt);

  return v.result;
}

CodegenResult CodegenVisitor::transform(const Pattern *ptr) {
  CodegenVisitor v(ctx);
  v.setSrcInfo(ptr->getSrcInfo());
  ptr->accept(v);
  v.result.addAttribute(kSrcInfoAttribute,
                        std::make_shared<SrcInfoAttribute>(v.getSrcInfo()));
  return v.result;
}

void CodegenVisitor::visit(const BoolExpr *expr) {
  result = CodegenResult(Ns<LiteralOperand>(expr->value));
}

void CodegenVisitor::visit(const IntExpr *expr) {
  try {
    if (expr->suffix == "u") {
      uint64_t i = std::stoull(expr->value, nullptr, 0);
      result = CodegenResult(Ns<LiteralOperand>(i));
    } else {
      int64_t i = std::stoull(expr->value, nullptr, 0);
      result = CodegenResult(Ns<LiteralOperand>(i));
    }
  } catch (std::out_of_range &) {
    error(getSrcInfo(), fmt::format("integer {} out of range",
                                    expr->value)
                            .c_str()); /// TODO: move to transform
  }
}

void CodegenVisitor::visit(const FloatExpr *expr) {
  result = CodegenResult(Ns<LiteralOperand>(expr->value));
}

void CodegenVisitor::visit(const StringExpr *expr) {
  result = CodegenResult(Ns<LiteralOperand>(expr->value));
}

shared_ptr<LLVMItem::Item>
CodegenVisitor::processIdentifier(shared_ptr<LLVMContext> tctx,
                                  const string &id) {
  auto val = tctx->find(id);
  if (!val)
    error(getSrcInfo(), fmt::format("? val {}", id).c_str());
  assert(val);
  // assert(
  // !(val->getVar() && val->isGlobal() && val->getBase() != ctx->getBase()));
  return val;
}

void CodegenVisitor::visit(const IdExpr *expr) {
  auto i = processIdentifier(ctx, expr->value);
  // TODO: this makes no sense: why setAtomic on temporary expr?
  // if (var->isGlobal() && var->getBase() == ctx->getBase() &&
  //     ctx->hasFlag("atomic"))
  //   dynamic_cast<seq::VarExpr *>(i->getExpr())->setAtomic();
  result = CodegenResult(i->getOperand());
}

void CodegenVisitor::visit(const TupleExpr *expr) {
  //  vector<seq::Expr *> items;
  //  for (auto &&i : expr->items)
  //    items.push_back(transform(i));
  //  resultExpr = N<seq::RecordExpr>(items, vector<string>(items.size(), ""));
  // TODO fix tuple expr
  internalError("TupleExpr codegen not supported");
}

void CodegenVisitor::visit(const IfExpr *expr) {
  auto var = Ns<ir::Var>(realizeType(expr->getType()->getClass()));
  // ctx->addVar(var->referenceString(), var);
  ctx->getBase()->addVar(var);

  auto condResultOp = toOperand(transform(expr->cond));

  auto nextBlock = newBlock();
  auto tBlock = newBlock();
  auto fBlock = newBlock();

  ctx->getBlock()->setTerminator(
      Ns<CondJumpTerminator>(tBlock, fBlock, condResultOp));

  ctx->addBlock(tBlock);
  auto tResultOp = toOperand(transform(expr->eif));
  tBlock->add(
      Ns<AssignInstr>(Ns<VarLvalue>(var), Ns<OperandRvalue>(tResultOp)));
  tBlock->setTerminator(Ns<JumpTerminator>(nextBlock));
  ctx->popBlock();

  ctx->addBlock(fBlock);
  auto fResultOp = toOperand(transform(expr->eelse));
  fBlock->add(
      Ns<AssignInstr>(Ns<VarLvalue>(var), Ns<OperandRvalue>(fResultOp)));
  fBlock->setTerminator(Ns<JumpTerminator>(nextBlock));
  ctx->popBlock();

  ctx->popBlock();
  ctx->addBlock(nextBlock);

  result = CodegenResult(Ns<VarOperand>(var));
}

void CodegenVisitor::visit(const UnaryExpr *expr) {
  // TODO fix unary expr
  internalError("UnaryExpr not supported");
}

void CodegenVisitor::visit(const BinaryExpr *expr) {
  assert(expr->op == "&&" || expr->op == "||");

  auto var = Ns<ir::Var>(ir::types::kBoolType);
  // ctx->addVar(var->referenceString(), var);
  ctx->getBase()->addVar(var);

  auto lhsOp = toOperand(transform(expr->lexpr));

  auto nextBlock = newBlock();
  auto trueBlock = newBlock();
  auto falseBlock = newBlock();

  auto trueOperand = Ns<LiteralOperand>(true);
  auto falseOperand = Ns<LiteralOperand>(false);

  if (expr->op == "&&") {
    auto tLeftBlock = newBlock();
    ctx->getBlock()->setTerminator(
        Ns<CondJumpTerminator>(tLeftBlock, falseBlock, lhsOp));

    ctx->addBlock(tLeftBlock);
    auto rhsOp = toOperand(transform(expr->rexpr));
    tLeftBlock->setTerminator(
        Ns<CondJumpTerminator>(trueBlock, falseBlock, rhsOp));
    ctx->popBlock();
  } else {
    auto fLeftBlock = newBlock();
    ctx->getBlock()->setTerminator(
        Ns<CondJumpTerminator>(trueBlock, fLeftBlock, lhsOp));

    ctx->addBlock(fLeftBlock);
    auto rhsOp = toOperand(transform(expr->rexpr));
    fLeftBlock->setTerminator(
        Ns<CondJumpTerminator>(trueBlock, falseBlock, rhsOp));
    ctx->popBlock();
  }

  ctx->addBlock(trueBlock);
  trueBlock->add(
      Ns<AssignInstr>(Ns<VarLvalue>(var), Ns<OperandRvalue>(trueOperand)));
  trueBlock->setTerminator(Ns<JumpTerminator>(nextBlock));
  ctx->popBlock();

  ctx->addBlock(falseBlock);
  falseBlock->add(
      Ns<AssignInstr>(Ns<VarLvalue>(var), Ns<OperandRvalue>(falseOperand)));
  falseBlock->setTerminator(Ns<JumpTerminator>(nextBlock));
  ctx->popBlock();

  ctx->popBlock();
  ctx->addBlock(nextBlock);

  result = CodegenResult(Ns<VarOperand>(var));
}

void CodegenVisitor::visit(const PipeExpr *expr) {
  vector<shared_ptr<Operand>> ops;
  vector<bool> parallel;
  for (const auto &item : expr->items) {
    ops.push_back(toOperand(transform(item.expr)));
    parallel.push_back(item.op == "||>");
  }
  result = CodegenResult(Ns<PipelineRvalue>(ops, parallel));
}

void CodegenVisitor::visit(const TupleIndexExpr *expr) {
  // TODO fix tuple index expr
  internalError("TupleIndexExpr codegen not supported");
}

void CodegenVisitor::visit(const CallExpr *expr) {
  // TODO fix partial call
  auto lhs = toOperand(transform(expr->expr));
  vector<shared_ptr<Operand>> items;
  bool isPartial = false;
  for (auto &&i : expr->args) {
    items.push_back(toOperand(transform(i.value)));
    isPartial |= !items.back();
  }
  if (isPartial)
    internalError("Partial call codegen not supported");
  else
    result = CodegenResult(Ns<CallRValue>(lhs, items));
}

void CodegenVisitor::visit(const StackAllocExpr *expr) {
  // TODO fix stack alloc
  internalError("StackAllocExpr codegen not supported");
}

void CodegenVisitor::visit(const DotExpr *expr) {
  if (auto c = CAST(expr->expr, IdExpr))
    if (c->value.size() && c->value[0] == '/') {
      auto ictx = ctx->getImports()->getImport(c->value.substr(1))->lctx;
      assert(ictx);
      result =
          CodegenResult(processIdentifier(ictx, expr->member)->getOperand());
      return;
    }
  result = CodegenResult(
      Ns<MemberRvalue>(toOperand(transform(expr->expr)), expr->member));
}

// void CodegenVisitor::visit(const EllipsisExpr *expr) {}

void CodegenVisitor::visit(const PtrExpr *expr) {
  // TODO fix ptr expr
  internalError("PtrExpr codegen not supported");
}

void CodegenVisitor::visit(const YieldExpr *expr) {
  // TODO fix yield expr
  internalError("YieldExpr codegen not supported");
}

void CodegenVisitor::visit(const SuiteStmt *stmt) {
  for (auto &s : stmt->stmts)
    transform(s);
}

void CodegenVisitor::visit(const PassStmt *stmt) {}

void CodegenVisitor::visit(const BreakStmt *stmt) {
  auto loop = std::static_pointer_cast<LoopAttribute>(
      ctx->getBlock()->getAttribute(kLoopAttribute));
  auto dst = loop->end.lock();
  if (!dst)
    internalError("No loop end");
  ctx->getBlock()->setTerminator(Ns<JumpTerminator>(dst));
}

void CodegenVisitor::visit(const ContinueStmt *stmt) {
  auto loop = std::static_pointer_cast<LoopAttribute>(
      ctx->getBlock()->getAttribute(kLoopAttribute));
  auto dst = loop->cond.lock();
  if (!dst)
    dst = loop->begin.lock();
  if (!dst)
    internalError("No loop body");
  ctx->getBlock()->setTerminator(Ns<JumpTerminator>(dst));
}

void CodegenVisitor::visit(const ExprStmt *stmt) {
  ctx->getBlock()->add(Ns<RvalueInstr>(toRvalue(transform(stmt->expr))));
}

void CodegenVisitor::visit(const AssignStmt *stmt) {
  /// TODO: atomic operations & JIT
  auto i = CAST(stmt->lhs, IdExpr);
  assert(i);
  auto var = i->value;
  // is it variable?
  if (stmt->rhs->isType()) {
    ctx->addType(var, realizeType(stmt->rhs->getType()->getClass()));
  } else {
    auto v = Ns<ir::Var>(var, realizeType(stmt->rhs->getType()->getClass()));
    if (ctx->isToplevel()) {
      ctx->getModule()->addGlobal(v);
    } else {
      ctx->getBase()->addVar(v);
    }
    ctx->addVar(var, v);
    ctx->getBlock()->add(
        Ns<AssignInstr>(Ns<VarLvalue>(v), toRvalue(transform(stmt->rhs))));
  }
}

void CodegenVisitor::visit(const AssignMemberStmt *stmt) {
  auto i = CAST(stmt->lhs, IdExpr);
  auto name = i->value;
  auto var = ctx->find(name, false);
  ctx->getBlock()->add(
      Ns<AssignInstr>(Ns<VarMemberLvalue>(var->getVar()->getHandle(), name),
                      toRvalue(transform(stmt->rhs))));
}

void CodegenVisitor::visit(const UpdateStmt *stmt) {
  auto i = CAST(stmt->lhs, IdExpr);
  assert(i);
  auto var = i->value;
  auto val = ctx->find(var, true);
  assert(val && val->getVar());
  ctx->getBlock()->add(
      Ns<AssignInstr>(Ns<VarLvalue>(val->getVar()->getHandle()),
                      toRvalue(transform(stmt->rhs))));
}

void CodegenVisitor::visit(const DelStmt *stmt) {
  auto expr = CAST(stmt->expr, IdExpr);
  assert(expr);
  auto v = ctx->find(expr->value, true)->getVar();
  assert(v);
  ctx->remove(expr->value);
}

void CodegenVisitor::visit(const PrintStmt *stmt) {
  internalError("PrintStmt codegen not supported");
}

void CodegenVisitor::visit(const ReturnStmt *stmt) {
  auto retVal = stmt->expr ? toOperand(transform(stmt->expr)) : nullptr;
  ctx->getBlock()->setTerminator(Ns<ReturnTerminator>(retVal));
}

void CodegenVisitor::visit(const YieldStmt *stmt) {
  auto dst = newBlock();
  auto yieldVal = stmt->expr ? toOperand(transform(stmt->expr)) : nullptr;
  ctx->getBlock()->setTerminator(Ns<YieldTerminator>(dst, yieldVal, std::weak_ptr<ir::Var>()));
  ctx->popBlock();
  ctx->addBlock(dst);
}

void CodegenVisitor::visit(const AssertStmt *stmt) {
  internalError("AssertStmt codegen not supported");
}

void CodegenVisitor::visit(const WhileStmt *stmt) {
  auto cond = newBlock();
  auto begin = newBlock();
  auto end = newBlock();
  begin->setAttribute(kLoopAttribute,
                      Ns<LoopAttribute>(std::weak_ptr<BasicBlock>(), cond, begin, std::weak_ptr<BasicBlock>(), end));

  ctx->addBlock(cond);
  ctx->getBlock()->setTerminator(
      Ns<CondJumpTerminator>(begin, end, toOperand(transform(stmt->cond))));
  ctx->popBlock();

  ctx->addBlock(begin);
  transform(stmt->suite);
  condSetTerminator(Ns<JumpTerminator>(cond));
  ctx->popBlock();

  ctx->getBlock()->setTerminator(Ns<JumpTerminator>(cond));
  ctx->popBlock();

  ctx->addBlock(end);
}

void CodegenVisitor::visit(const ForStmt *stmt) {
  auto setupCheck = newBlock();
  auto setupAction = newBlock();
  auto cond = newBlock();
  auto begin = newBlock();
  auto update = newBlock();
  auto end = newBlock();

  begin->setAttribute(kLoopAttribute,
                      Ns<LoopAttribute>(setupCheck, cond, begin, update, end));

  auto expr = CAST(stmt->var, IdExpr);
  auto var = Ns<ir::Var>(expr->value, realizeType(expr->getType()->getClass()));
  ctx->getBase()->addVar(var);
  ctx->addVar(expr->value, var);

  auto doneFunc = CAST(stmt->done, IdExpr);
  auto nextFunc = CAST(stmt->done, IdExpr);

  auto doneCheckVar = Ns<ir::Var>(ir::types::kBoolType);
  ctx->getBase()->addVar(doneCheckVar);

  auto gen = Ns<ir::Var>(realizeType(stmt->iter->getType()->getClass()));
  ctx->getBase()->addVar(gen);

  // setup
  ctx->addBlock(setupCheck);
  ctx->getBlock()->add(Ns<AssignInstr>(Ns<VarLvalue>(gen),
      toRvalue(transform(stmt->iter))));
  ctx->getBlock()->add(Ns<AssignInstr>(Ns<VarLvalue>(doneCheckVar),
      Ns<CallRValue>(toOperand(transform(doneFunc)),
     std::vector<std::shared_ptr<Operand>>({ Ns<VarOperand>(gen) }))));
  ctx->getBlock()->setTerminator(Ns<CondJumpTerminator>(
      end, setupAction, Ns<VarOperand>(doneCheckVar)));
  ctx->popBlock();

  ctx->addBlock(setupAction);
  ctx->getBlock()->add(Ns<AssignInstr>(Ns<VarLvalue>(var),
                                       Ns<CallRValue>(toOperand(transform(nextFunc)),
                                                      std::vector<std::shared_ptr<Operand>>({ Ns<VarOperand>(gen) }))));
  ctx->getBlock()->setTerminator(Ns<JumpTerminator>(begin));
  ctx->popBlock();


  // condition
  ctx->addBlock(cond);
  ctx->getBlock()->add(Ns<AssignInstr>(Ns<VarLvalue>(doneCheckVar),
                                       Ns<CallRValue>(toOperand(transform(doneFunc)),
                                                      std::vector<std::shared_ptr<Operand>>({ Ns<VarOperand>(gen) }))));
  ctx->getBlock()->setTerminator(Ns<CondJumpTerminator>(
      end, setupAction, Ns<VarOperand>(doneCheckVar)));
  ctx->popBlock();

  // first block
  ctx->addBlock(begin);
  transform(stmt->suite);
  condSetTerminator(Ns<JumpTerminator>(update));
  ctx->popBlock();

  // update
  ctx->addBlock(update);
  ctx->getBlock()->add(Ns<AssignInstr>(Ns<VarLvalue>(var),
                                       Ns<CallRValue>(toOperand(transform(nextFunc)),
                                                      std::vector<std::shared_ptr<Operand>>({ Ns<VarOperand>(gen) }))));
  ctx->getBlock()->setTerminator(Ns<JumpTerminator>(cond));
  ctx->popBlock();

  ctx->getBlock()->setTerminator(Ns<JumpTerminator>(setupCheck));
  ctx->popBlock();

  ctx->addBlock(end);
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
      check->setTerminator(
          Ns<CondJumpTerminator>(body, newCheck, toOperand(transform(i.cond))));
      ctx->popBlock();

      ctx->addBlock(body);
      transform(i.suite);
      condSetTerminator(Ns<JumpTerminator>(end));
      ctx->popBlock();

      check = newCheck;
    } else {
      elseEncountered = true;

      auto body = check;
      ctx->addBlock(body);
      transform(i.suite);
      condSetTerminator(Ns<JumpTerminator>(end));
      ctx->popBlock();
    }
  }

  if (!elseEncountered) {
    ctx->addBlock(check);
    ctx->getBlock()->setTerminator(Ns<JumpTerminator>(end));
    ctx->popBlock();
  }

  ctx->getBlock()->setTerminator(Ns<JumpTerminator>(firstCond));
  ctx->popBlock();
  ctx->addBlock(end);
}

void CodegenVisitor::visit(const MatchStmt *stmt) {
  auto firstCond = newBlock();
  auto end = newBlock();
  auto check = firstCond;

  for (auto ci = 0; ci < stmt->cases.size(); ci++) {
    string varName;
    shared_ptr<ir::Var> var;
    shared_ptr<ir::Pattern> pat;
    if (auto p = CAST(stmt->patterns[ci], BoundPattern)) {
      ctx->addBlock();
      auto boundPat = Ns<ir::BoundPattern>(transform(p->pattern).patternResult);
      var = boundPat->getVar();
      varName = p->var;
      pat = std::static_pointer_cast<ir::Pattern>(boundPat);
      ctx->popBlock();
    } else {
      ctx->addBlock();
      pat = transform(stmt->patterns[ci]).patternResult;
      ctx->popBlock();
    }

    auto newCheck = newBlock();
    auto body = newBlock();

    ctx->addBlock(check);
    auto t = Ns<ir::Var>(ir::types::kBoolType);
    ctx->getBase()->addVar(t);
    ctx->getBlock()->add(Ns<AssignInstr>(
        Ns<VarLvalue>(t),
        Ns<MatchRvalue>(pat, toOperand(transform(stmt->what)))));
    check->setTerminator(
        Ns<CondJumpTerminator>(body, newCheck, Ns<VarOperand>(t)));
    ctx->popBlock();

    if (var)
      ctx->addVar(varName, var);

    ctx->addBlock(body);
    transform(stmt->cases[ci]);
    condSetTerminator(Ns<JumpTerminator>(end));
    ctx->popBlock();

    check = newCheck;
  }

  ctx->addBlock(check);
  ctx->getBlock()->setTerminator(Ns<JumpTerminator>(end));
  ctx->popBlock();

  ctx->getBlock()->setTerminator(Ns<JumpTerminator>(firstCond));

  ctx->popBlock();
  ctx->addBlock(end);
}

void CodegenVisitor::visit(const ImportStmt *stmt) {
  auto file = stmt->from.first.substr(1);
  // ctx->getImports()->getImportFile(stmt->from.first, ctx->getFilename());
  // assert(!file.empty());

  auto import =
      const_cast<ImportContext::Import *>(ctx->getImports()->getImport(file));
  assert(import);
  if (!import->lctx) {
    import->lctx = make_shared<LLVMContext>(
        file, ctx->getRealizations(), ctx->getImports(), ctx->getBlock(),
        ctx->getBase(), ctx->getModule(), ctx->getJIT());
    CodegenVisitor(import->lctx).transform(import->statements);
  }

  if (!stmt->what.size()) {
    ctx->addImport(
        stmt->from.second == "" ? stmt->from.first : stmt->from.second, file);
  } else if (stmt->what.size() == 1 && stmt->what[0].first == "*") {
    for (auto &i : *(import->lctx))
      ctx->add(i.first, i.second.front());
  } else {
    for (auto &w : stmt->what) {
      for (auto i : *(import->lctx)) {
        if (i.second.front()->getVar() && i.first == w.first)
          ctx->add(i.first, i.second.front());
      }
      // for (auto &w : stmt->what) {
      // TODO: those should have been already resolved  ... ?
      // for (auto i : *(import->lctx)) {
      //   if (i.first.substr(0, w.first.size() + 1) == w.first + ".")
      //     ctx->add(i.first, i.second.front());
      // }
      // auto c = import->lctx->find(w.first);
      // assert(c);
      // ctx->add(w.second == "" ? w.first : w.second, c);
      // }
    }
  }
}

void CodegenVisitor::visit(const TryStmt *stmt) {
  auto parentTc = ctx->getBlock()->getAttribute(kTryCatchAttribute)
                      ? std::static_pointer_cast<TryCatchAttribute>(
                            ctx->getBlock()->getAttribute(kTryCatchAttribute))
                            ->handler
                      : nullptr;
  auto newTc = Ns<ir::TryCatch>();
  if (parentTc)
    parentTc->addChild(newTc);

  auto end = newBlock();
  auto body = newBlock();
  body->setAttribute(kTryCatchAttribute, Ns<TryCatchAttribute>(newTc));

  ctx->addBlock(body);
  transform(stmt->suite);
  condSetTerminator(Ns<JumpTerminator>(end));
  ctx->popBlock();

  int varIdx = 0;
  for (auto &c : stmt->catches) {
    /// TODO: get rid of typeinfo here?
    auto cBlock = newBlock();
    ctx->addBlock(cBlock);
    transform(c.suite);
    condSetTerminator(Ns<JumpTerminator>(end));
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

void CodegenVisitor::visit(const GlobalStmt *stmt) {
  auto var = ctx->find(stmt->var)->getVar();
  assert(var && var->isGlobal() && var->getBase() == ctx->getBase());
  ctx->addVar(stmt->var, var->getHandle(), true);
}

void CodegenVisitor::visit(const ThrowStmt *stmt) {
  ctx->getBlock()->setTerminator(
      Ns<ThrowTerminator>(toOperand(transform(stmt->expr))));
}

void CodegenVisitor::visit(const FunctionStmt *stmt) {
  auto name = ctx->getRealizations()->getCanonicalName(stmt->getSrcInfo());
  for (auto &real : ctx->getRealizations()->getFuncRealizations(name)) {
    // TODO - change realization to use smart pointers
    auto f = std::static_pointer_cast<ir::Func>(real.handle->getShared());
    assert(f);
    if (in(real.ast->attributes, "internal"))
      continue;
    // LOG7("[codegen] generating fn {}", real.fullName);
    auto name = chop(real.fullName);
    f->setName(name);
    f->setAttribute(kSrcInfoAttribute, Ns<SrcInfoAttribute>(getSrcInfo()));
    if (!ctx->isToplevel())
      f->setEnclosingFunc(ctx->getBase());
    ctx->getModule()->addGlobal(f);
    ctx->addBlock(f->getBlocks()[0], f);
    vector<string> names;
    vector<shared_ptr<ir::types::Type>> types;
    for (int i = 1; i < real.type->args.size(); i++) {
      types.push_back(realizeType(real.type->args[i]->getClass()));
      names.push_back(real.ast->args[i - 1].name);
    }
    auto funcType = realizeType(real.type);
    f->setType(funcType);
    f->setArgNames(names);
    f->setAttribute(kFuncAttribute, Ns<FuncAttribute>(real.ast->attributes));
    for (auto a : real.ast->attributes) {
      if (a == "atomic")
        ctx->setFlag("atomic");
    }
    if (in(real.ast->attributes, "$external")) {
      f->setName(chop(real.ast->name));
      f->setExternal();
    } else {
      for (auto &arg : names)
        ctx->addVar(arg, f->getArgVar(arg));
      transform(real.ast->suite.get());
    }
    ctx->popBlock();
  }
}

void CodegenVisitor::visitMethods(const string &name) {
  auto c = ctx->getRealizations()->findClass(name);
  if (c)
    for (auto &m : c->methods)
      for (auto &mm : m.second) {
        FunctionStmt *f = CAST(
            ctx->getRealizations()->getAST(mm->canonicalName), FunctionStmt);
        visit(f);
      }
}

void CodegenVisitor::visit(const ClassStmt *stmt) {
  // visitMethods(ctx->getRealizations()->getCanonicalName(stmt->getSrcInfo()));
}

void CodegenVisitor::visit(const StarPattern *pat) {
  result = CodegenResult(Ns<ir::StarPattern>());
}

void CodegenVisitor::visit(const IntPattern *pat) {
  result = CodegenResult(Ns<ir::IntPattern>(pat->value));
}

void CodegenVisitor::visit(const BoolPattern *pat) {
  result = CodegenResult(Ns<ir::BoolPattern>(pat->value));
}

void CodegenVisitor::visit(const StrPattern *pat) {
  result = CodegenResult(Ns<ir::StrPattern>(pat->value));
}

void CodegenVisitor::visit(const SeqPattern *pat) {
  result = CodegenResult(Ns<ir::SeqPattern>(pat->value));
}

void CodegenVisitor::visit(const RangePattern *pat) {
  result = CodegenResult(Ns<ir::RangePattern>(pat->start, pat->end));
}

void CodegenVisitor::visit(const TuplePattern *pat) {
  vector<shared_ptr<ir::Pattern>> pp;
  for (auto &p : pat->patterns)
    pp.push_back(transform(p).patternResult);
  result = CodegenResult(Ns<ir::RecordPattern>(move(pp)));
}

void CodegenVisitor::visit(const ListPattern *pat) {
  vector<shared_ptr<ir::Pattern>> pp;
  for (auto &p : pat->patterns)
    pp.push_back(transform(p).patternResult);
  result = CodegenResult(Ns<ir::ArrayPattern>(move(pp)));
}

void CodegenVisitor::visit(const OrPattern *pat) {
  vector<shared_ptr<ir::Pattern>> pp;
  for (auto &p : pat->patterns)
    pp.push_back(transform(p).patternResult);
  result = CodegenResult(Ns<ir::OrPattern>(move(pp)));
}

void CodegenVisitor::visit(const WildcardPattern *pat) {
  auto p = Ns<ir::WildcardPattern>();
  ctx->addVar(pat->var, p->getVar());
  result = CodegenResult(p);
}

void CodegenVisitor::visit(const GuardedPattern *pat) {
  result = CodegenResult(Ns<ir::GuardedPattern>(
      transform(pat->pattern).patternResult, toOperand(transform(pat->cond))));
}

shared_ptr<ir::types::Type> CodegenVisitor::realizeType(types::ClassTypePtr t) {
  // DBG("looking for {} / {}", t->name, t->toString(true));
  assert(t && t->canRealize());
  auto it = ctx->getRealizations()->classRealizations.find(t->name);
  assert(it != ctx->getRealizations()->classRealizations.end());
  auto it2 = it->second.find(t->realizeString(t->name, false));
  assert(it2 != it->second.end());
  assert(it2->second.handle);
  return it2->second.handle->getShared();
}

} // namespace ast
} // namespace seq
