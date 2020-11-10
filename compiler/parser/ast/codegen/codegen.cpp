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
  auto templateBlock = ctx->getBlock();
  auto ret = make_shared<BasicBlock>(templateBlock->getTryCatch(),
                                     templateBlock->isCatchClause());
  ctx->getBase()->addBlock(ret);

  if (templateBlock->getAttribute(kLoopAttribute))
    ret->setAttribute(kLoopAttribute,
                      make_shared<LoopAttribute>(
                          *templateBlock->getAttribute<LoopAttribute>(kLoopAttribute)));

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
    ctx->getBlock()->append(
        Ns<AssignInstr>(srcInfo, Ns<VarLvalue>(srcInfo, t), res.rvalueResult));
    return Ns<VarOperand>(srcInfo, t);
  }
  case CodegenResult::PATTERN:
    seqassert(false, "cannot convert pattern to operand.");
    return nullptr;
  case CodegenResult::TYPE:
    seqassert(false, "cannot convert type to operand.");
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

shared_ptr<SIRModule> CodegenVisitor::apply(shared_ptr<Cache> cache, StmtPtr stmts) {
  auto module = make_shared<SIRModule>("module");
  auto block = module->getMain()->getBlocks()[0];
  auto ctx =
      make_shared<CodegenContext>(cache, block, module, module->getMain(), nullptr);

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
          auto fn = Ns<ir::Func>(t->getSrcInfo(), ast->name, vector<string>(),
                                 ir::types::kNoArgVoidFuncType);
          fn->setInternal(typ, name);
          ctx->functions[f.first] = {fn, false};
          ctx->getModule()->addGlobal(fn);
        } else {
          auto fn = Ns<ir::Func>(t->getSrcInfo(), ast->name, vector<string>(),
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

  if (auto v = val->getVar())
    result = CodegenResult(Ns<VarOperand>(expr->getSrcInfo(), v));
  else if (auto f = val->getFunc())
    result = CodegenResult(Ns<VarOperand>(expr->getSrcInfo(), f));
  else
    result = CodegenResult(val->getType());
}

void CodegenVisitor::visit(const IfExpr *expr) {
  // Create a temporary variable to store the result of the if expr.
  auto ifExprResVar = Ns<ir::Var>(expr->getSrcInfo(), temporaryName("if_res"),
                                  realizeType(expr->getType()->getClass()));
  ctx->getBase()->addVar(ifExprResVar);

  // Create blocks for the true and fall case.
  auto tBlock = newBlock();
  auto fBlock = newBlock();

  // Create a done block.
  auto doneBlock = newBlock();

  // Jump based on the cond
  auto condResultOp = toOperand(transform(expr->cond));
  ctx->getBlock()->setTerminator(
      Ns<CondJumpTerminator>(expr->getSrcInfo(), tBlock, fBlock, condResultOp));

  // Assign in true block
  ctx->addBlock(tBlock);
  auto tResultOp = toOperand(transform(expr->eif));
  ctx->getBlock()->append(Ns<AssignInstr>(
      expr->getSrcInfo(), Ns<VarLvalue>(expr->getSrcInfo(), ifExprResVar),
      Ns<OperandRvalue>(expr->getSrcInfo(), tResultOp)));
  ctx->getBlock()->setTerminator(Ns<JumpTerminator>(expr->getSrcInfo(), doneBlock));
  ctx->popBlock();

  // Assign in false block
  ctx->addBlock(fBlock);
  auto fResultOp = toOperand(transform(expr->eelse));
  ctx->getBlock()->append(Ns<AssignInstr>(
      expr->getSrcInfo(), Ns<VarLvalue>(expr->getSrcInfo(), ifExprResVar),
      Ns<OperandRvalue>(expr->getSrcInfo(), fResultOp)));
  ctx->getBlock()->setTerminator(Ns<JumpTerminator>(expr->getSrcInfo(), doneBlock));
  ctx->popBlock();

  // Replace the current block with doneBlock
  ctx->replaceBlock(doneBlock);

  result = CodegenResult(Ns<VarOperand>(expr->getSrcInfo(), ifExprResVar));
}

void CodegenVisitor::visit(const BinaryExpr *expr) {
  // Only support short circuit for and/or
  assert(expr->op == "&&" || expr->op == "||");

  // Create a temporary to store the result of the binary op
  auto binExprResVar =
      Ns<ir::Var>(expr->getSrcInfo(), temporaryName("bin_res"), ir::types::kBoolType);
  ctx->getBase()->addVar(binExprResVar);

  // Create blocks for the true/false case
  auto trueBlock = newBlock();
  auto falseBlock = newBlock();

  // Create a done block
  auto doneBlock = newBlock();

  // Transform the left hand side
  auto lhsOp = toOperand(transform(expr->lexpr));
  if (expr->op == "&&") {
    // Short circuit to falseBlock if left is false
    auto tLeftBlock = newBlock();
    ctx->getBlock()->setTerminator(
        Ns<CondJumpTerminator>(expr->getSrcInfo(), tLeftBlock, falseBlock, lhsOp));

    // Jump to trueBlock if right is also true, otherwise falseBlock
    ctx->addBlock(tLeftBlock);
    auto rhsOp = toOperand(transform(expr->rexpr));
    ctx->getBlock()->setTerminator(
        Ns<CondJumpTerminator>(expr->getSrcInfo(), trueBlock, falseBlock, rhsOp));
    ctx->popBlock();
  } else {
    // Short circuit to trueBlock if left is true
    auto fLeftBlock = newBlock();
    ctx->getBlock()->setTerminator(
        Ns<CondJumpTerminator>(expr->getSrcInfo(), trueBlock, fLeftBlock, lhsOp));

    // Jump to trueBlock is right is true, otherwise falseBlock
    ctx->addBlock(fLeftBlock);
    auto rhsOp = toOperand(transform(expr->rexpr));
    ctx->getBlock()->setTerminator(
        Ns<CondJumpTerminator>(expr->getSrcInfo(), trueBlock, falseBlock, rhsOp));
    ctx->popBlock();
  }

  // Set the result to true in trueBlock
  ctx->addBlock(trueBlock);
  auto trueOperand = Ns<LiteralOperand>(expr->getSrcInfo(), true);
  ctx->getBlock()->append(Ns<AssignInstr>(
      expr->getSrcInfo(), Ns<VarLvalue>(expr->getSrcInfo(), binExprResVar),
      Ns<OperandRvalue>(expr->getSrcInfo(), trueOperand)));
  ctx->getBlock()->setTerminator(Ns<JumpTerminator>(expr->getSrcInfo(), doneBlock));
  ctx->popBlock();

  // Set the result to false in falseBlock
  ctx->addBlock(falseBlock);
  auto falseOperand = Ns<LiteralOperand>(expr->getSrcInfo(), false);
  ctx->getBlock()->append(Ns<AssignInstr>(
      expr->getSrcInfo(), Ns<VarLvalue>(expr->getSrcInfo(), binExprResVar),
      Ns<OperandRvalue>(expr->getSrcInfo(), falseOperand)));
  ctx->getBlock()->setTerminator(Ns<JumpTerminator>(expr->getSrcInfo(), doneBlock));
  ctx->popBlock();

  // Replace the current block with doneBlock
  ctx->replaceBlock(doneBlock);

  result = CodegenResult(Ns<VarOperand>(expr->getSrcInfo(), binExprResVar));
}

void CodegenVisitor::visit(const PipeExpr *expr) {
  // Transform the first stage
  vector<shared_ptr<Operand>> ops = {toOperand(transform(expr->items[0].expr))};
  vector<bool> parallel = {expr->items[0].op == "||>"};
  vector<shared_ptr<ir::types::Type>> inTypes = {
      realizeType(expr->inTypes[0]->getClass())};

  // Transform subsequent stages
  for (auto i = 1; i < expr->items.size(); ++i) {
    auto e = CAST(expr->items[i].expr, CallExpr);
    assert(e);

    // Realize the args of the partial
    auto pfn = toOperand(transform(e->expr));
    vector<shared_ptr<Operand>> items(e->args.size(), nullptr);
    vector<string> names(e->args.size(), "");
    vector<shared_ptr<ir::types::Type>> partials(e->args.size(), nullptr);

    for (int ai = 0; ai < e->args.size(); ai++)
      if (!CAST(e->args[ai].value, EllipsisExpr)) {
        items[ai] = toOperand(transform(e->args[ai].value));
        partials[ai] = realizeType(e->args[ai].value->getType()->getClass());
      }

    // Setup the partial
    auto pType =
        realizeType(e->getType()->getClass())->as<ir::types::PartialFuncType>();
    auto tVar = Ns<ir::Var>(expr->getSrcInfo(), pType);
    ctx->getBase()->addVar(tVar);
    ctx->getBlock()->append(
        Ns<AssignInstr>(expr->getSrcInfo(), Ns<VarLvalue>(expr->getSrcInfo(), tVar),
                        Ns<PartialCallRvalue>(expr->getSrcInfo(), pfn, items, pType)));
    ops.push_back(Ns<VarOperand>(expr->getSrcInfo(), tVar));
    inTypes.push_back(realizeType(expr->inTypes[i]->getClass()));
  }

  result = CodegenResult(Ns<PipelineRvalue>(expr->getSrcInfo(), ops, parallel, inTypes,
                                            realizeType(expr->getType()->getClass())));
}

void CodegenVisitor::visit(const CallExpr *expr) {
  // Transform the function
  auto lhs = toOperand(transform(expr->expr));
  vector<shared_ptr<Operand>> items;

  // Transform the args
  for (auto &&i : expr->args) {
    if (CAST(i.value, EllipsisExpr)) {
      seqassert(false, "partials codegen only supported in pipelines");
    } else {
      items.push_back(toOperand(transform(i.value)));
    }
  }

  result = CodegenResult(Ns<CallRvalue>(expr->getSrcInfo(), lhs, items));

  // Necessary because function may still be stubbed
  result.typeOverride = realizeType(expr->getType()->getClass());
}

void CodegenVisitor::visit(const StackAllocExpr *expr) {
  auto arrayType = realizeType(expr->getType()->getClass())->as<ir::types::ArrayType>();
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

  auto pointerType =
      realizeType(expr->getType()->getClass())->as<ir::types::PointerType>();
  result = CodegenResult(Ns<VarPointerOperand>(expr->getSrcInfo(), pointerType, v));
}

void CodegenVisitor::visit(const YieldExpr *expr) {
  ctx->getBase()->setGenerator();

  auto var = Ns<ir::Var>(expr->getSrcInfo(), temporaryName("yield_res"),
                         realizeType(expr->getType()->getClass()));
  ctx->getBase()->addVar(var);

  auto dst = newBlock();
  ctx->getBlock()->setTerminator(Ns<YieldTerminator>(expr->getSrcInfo(), dst, var));

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
  ctx->getBlock()->append(Ns<RvalueInstr>(stmt->getSrcInfo(), rval));
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
    //    seqassert(false, "assigning types is not supported");
  } else {
    auto v = Ns<ir::Var>(stmt->getSrcInfo(), var,
                         realizeType(stmt->rhs->getType()->getClass()));
    if (var[0] == '.')
      ctx->getModule()->addGlobal(v);
    else
      ctx->getBase()->addVar(v);
    ctx->addVar(var, v, var[0] == '.');

    auto rval = toRvalue(transform(stmt->rhs));
    ctx->getBlock()->append(Ns<AssignInstr>(
        stmt->getSrcInfo(), Ns<VarLvalue>(stmt->getSrcInfo(), v), rval));
  }
}

void CodegenVisitor::visit(const AssignMemberStmt *stmt) {
  auto i = CAST(stmt->lhs, IdExpr);
  auto name = i->value;

  auto var = ctx->find(name, false);
  auto rhs = toRvalue(transform(stmt->rhs));
  ctx->getBlock()->append(Ns<AssignInstr>(
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
  ctx->getBlock()->append(Ns<AssignInstr>(
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
      Ns<YieldTerminator>(stmt->getSrcInfo(), dst, yieldVal));
  ctx->replaceBlock(dst);
}

void CodegenVisitor::visit(const AssertStmt *stmt) {
  auto dst = newBlock();
  auto op = toOperand(transform(stmt->expr));
  ctx->getBlock()->setTerminator(Ns<AssertTerminator>(stmt->getSrcInfo(), op, dst));
  ctx->replaceBlock(dst);
}

void CodegenVisitor::visit(const WhileStmt *stmt) {
  // Create blocks for the condition, body, and end
  auto cond = newBlock();
  auto begin = newBlock();
  auto doneBlock = newBlock();

  begin->setAttribute(kLoopAttribute,
                      make_shared<LoopAttribute>(weak_ptr<BasicBlock>(), cond, begin,
                                                 weak_ptr<BasicBlock>(), doneBlock));

  ctx->getBlock()->setTerminator(Ns<JumpTerminator>(stmt->getSrcInfo(), cond));

  // Add a new level for variables defined in the while
  ctx->addLevel();

  // Jump to end if cond is false, otherwise being
  ctx->addBlock(cond);
  auto condOp = toOperand(transform(stmt->cond));
  ctx->getBlock()->setTerminator(
      Ns<CondJumpTerminator>(stmt->getSrcInfo(), begin, doneBlock, condOp));
  ctx->popBlock();

  // Transform the loop body
  ctx->addBlock(begin);
  transform(stmt->suite);
  condSetTerminator(Ns<JumpTerminator>(stmt->getSrcInfo(), cond));
  ctx->popBlock();

  // Remove the new level
  ctx->removeLevel();

  // Replace the current block with doneBock
  ctx->replaceBlock(doneBlock);
}

void CodegenVisitor::visit(const ForStmt *stmt) {
  // Create blocks for setup, cond, loop body, and end
  auto setup = newBlock();
  auto cond = newBlock();
  auto begin = newBlock();
  auto doneBlock = newBlock();

  begin->setAttribute(kLoopAttribute,
                      make_shared<LoopAttribute>(setup, cond, begin,
                                                 weak_ptr<BasicBlock>(), doneBlock));

  ctx->getBlock()->setTerminator(Ns<JumpTerminator>(stmt->getSrcInfo(), setup));

  // Add a new level for variables in for
  ctx->addLevel();

  // Transform the generator object into the new level
  transform(stmt->iter);

  // Create a variable for the done condition
  auto doneVar =
      Ns<ir::Var>(stmt->getSrcInfo(), temporaryName("for_done"), ir::types::kBoolType);
  ctx->getBase()->addVar(doneVar);

  // Create a variable to hold the current object
  auto varId = CAST(stmt->var, IdExpr);
  auto resVar = Ns<ir::Var>(stmt->getSrcInfo(), varId->value,
                            realizeType(varId->getType()->getClass()));
  ctx->addVar(varId->value, resVar);
  ctx->getBase()->addVar(resVar);

  // Setup should initialize doneVar, then jump to cond
  ctx->addBlock(setup);
  auto doneSetupRval = toRvalue(transform(stmt->done));
  ctx->getBlock()->append(Ns<AssignInstr>(
      stmt->getSrcInfo(), Ns<VarLvalue>(stmt->getSrcInfo(), doneVar), doneSetupRval));
  ctx->getBlock()->setTerminator(Ns<JumpTerminator>(stmt->getSrcInfo(), cond));
  ctx->popBlock();

  // Cond should exit if done, jump to body if not
  ctx->addBlock(cond);
  ctx->getBlock()->setTerminator(
      Ns<CondJumpTerminator>(stmt->getSrcInfo(), doneBlock, begin,
                             Ns<VarOperand>(stmt->getSrcInfo(), doneVar)));
  ctx->popBlock();

  // Begin should assign resVar, run the loop body, then assign doneVar
  ctx->addBlock(begin);
  auto nextRval = toRvalue(transform(stmt->next));
  ctx->getBlock()->append(Ns<AssignInstr>(
      stmt->getSrcInfo(), Ns<VarLvalue>(stmt->getSrcInfo(), resVar), nextRval));
  transform(stmt->suite);

  auto doneRval = toRvalue(transform(stmt->done));
  ctx->getBlock()->append(Ns<AssignInstr>(
      stmt->getSrcInfo(), Ns<VarLvalue>(stmt->getSrcInfo(), doneVar), doneRval));
  condSetTerminator(Ns<JumpTerminator>(stmt->getSrcInfo(), cond));
  ctx->popBlock();

  // Remove the new level
  ctx->removeLevel();

  // Replace the current block with doneBlock
  ctx->replaceBlock(doneBlock);
}

void CodegenVisitor::visit(const IfStmt *stmt) {
  // Create blocks for the conditions
  auto firstCond = newBlock();
  auto check = firstCond;
  auto doneBlock = newBlock();

  bool elseEncountered = false;

  // Iterate over the conditions
  for (auto &i : stmt->ifs) {
    // Each condition gets its own level
    ctx->addLevel();

    // If not an else:
    if (i.cond) {
      // Allocate blocks for the body and next check
      auto newCheck = newBlock();
      auto body = newBlock();

      // In check, jump to body if the cond is true, otherwise newCheck
      ctx->addBlock(check);
      auto cond = toOperand(transform(i.cond));
      ctx->getBlock()->setTerminator(
          Ns<CondJumpTerminator>(stmt->getSrcInfo(), body, newCheck, cond));
      ctx->popBlock();

      // Codegen body
      ctx->addBlock(body);
      transform(i.suite);
      condSetTerminator(Ns<JumpTerminator>(stmt->getSrcInfo(), doneBlock));
      ctx->popBlock();

      check = newCheck;
    } else {
      seqassert(!elseEncountered, "only one else supported");
      elseEncountered = true;

      // Codegen else body
      ctx->addBlock(check);
      transform(i.suite);
      condSetTerminator(Ns<JumpTerminator>(stmt->getSrcInfo(), doneBlock));
      ctx->popBlock();
    }

    ctx->removeLevel();
  }

  // If no else, we need to direct check to the doneBlock
  if (!elseEncountered) {
    ctx->addBlock(check);
    ctx->getBlock()->setTerminator(Ns<JumpTerminator>(stmt->getSrcInfo(), doneBlock));
    ctx->popBlock();
  }

  // Jump to first condition
  ctx->getBlock()->setTerminator(Ns<JumpTerminator>(stmt->getSrcInfo(), firstCond));

  // Replace the current block with doneBlock
  ctx->replaceBlock(doneBlock);
}

void CodegenVisitor::visit(const MatchStmt *stmt) {
  // Create blocks for the matches
  auto firstCond = newBlock();
  auto check = firstCond;
  auto doneBlock = newBlock();

  ctx->getBlock()->setTerminator(Ns<JumpTerminator>(stmt->getSrcInfo(), firstCond));

  for (auto ci = 0; ci < stmt->cases.size(); ci++) {
    // Each match gets its own level
    ctx->addLevel();

    string varName;
    shared_ptr<ir::Var> var;
    shared_ptr<ir::Pattern> pat;

    // Similar to if statement
    auto newCheck = newBlock();
    auto body = newBlock();

    ctx->addBlock(check);
    if (auto p = CAST(stmt->patterns[ci], BoundPattern)) {
      auto boundPat =
          Ns<ir::BoundPattern>(stmt->getSrcInfo(), transform(p->pattern).patternResult,
                               realizeType(p->pattern->getType()->getClass()));
      var = boundPat->getVar();
      ctx->getBase()->addVar(var);
      varName = p->var;

      pat = boundPat;
    } else {
      pat = transform(stmt->patterns[ci]).patternResult;
    }

    auto t = Ns<ir::Var>(stmt->getSrcInfo(), temporaryName("match_res"),
                         ir::types::kBoolType);
    ctx->getBase()->addVar(t);

    auto matchOp = toOperand(transform(stmt->what));
    ctx->getBlock()->append(
        Ns<AssignInstr>(stmt->getSrcInfo(), Ns<VarLvalue>(stmt->getSrcInfo(), t),
                        Ns<MatchRvalue>(stmt->getSrcInfo(), pat, matchOp)));

    // Jump to body if match
    ctx->getBlock()->setTerminator(Ns<CondJumpTerminator>(
        stmt->getSrcInfo(), body, newCheck, Ns<VarOperand>(stmt->getSrcInfo(), t)));
    ctx->popBlock();

    if (var)
      ctx->addVar(varName, var);

    // Transform the body
    ctx->addBlock(body);
    transform(stmt->cases[ci]);
    condSetTerminator(Ns<JumpTerminator>(stmt->getSrcInfo(), doneBlock));
    ctx->popBlock();

    check = newCheck;

    ctx->removeLevel();
  }

  // No catch all
  ctx->addBlock(check);
  ctx->getBlock()->setTerminator(Ns<JumpTerminator>(stmt->getSrcInfo(), doneBlock));
  ctx->popBlock();

  // Replace current block with doneBlock
  ctx->replaceBlock(doneBlock);
}

void CodegenVisitor::visit(const TryStmt *stmt) {
  auto parentTc = ctx->getBlock()->getTryCatch() ? ctx->getBlock()->getTryCatch()
                                                 : std::shared_ptr<ir::TryCatch>();
  auto newTc = Ns<ir::TryCatch>(stmt->getSrcInfo());

  if (parentTc)
    parentTc->addChild(newTc);
  else
    ctx->getBase()->addTryCatch(newTc);

  // Add the flag variable and initialize it to 0
  ctx->getBase()->addVar(newTc->getFlagVar());
  ctx->getBlock()->append(Ns<AssignInstr>(
      stmt->getSrcInfo(), Ns<VarLvalue>(stmt->getSrcInfo(), newTc->getFlagVar()),
      Ns<OperandRvalue>(stmt->getSrcInfo(),
                        Ns<LiteralOperand>(stmt->getSrcInfo(), int64_t(0)))));

  // Create blocks for the body and end block
  auto body = newBlock();
  auto doneBlock = newBlock();

  ctx->getBlock()->setTerminator(Ns<JumpTerminator>(stmt->getSrcInfo(), body));

  // Set the try catch of the body
  body->setTryCatch(newTc);

  // Body gets its own level
  ctx->addLevel();

  ctx->addBlock(body);
  transform(stmt->suite);
  condSetTerminator(Ns<JumpTerminator>(stmt->getSrcInfo(), doneBlock));
  ctx->popBlock();

  ctx->removeLevel();

  int varIdx = 0;
  for (auto i = 0; i < stmt->catches.size(); ++i) {
    auto &c = stmt->catches[i];

    auto cBlock = newBlock();
    cBlock->setTryCatch(newTc, true);

    newTc->addCatch(c.exc ? realizeType(c.exc->getType()->getClass()) : nullptr, c.var,
                    cBlock);

    // Each catch gets its own level
    ctx->addLevel();
    if (!c.var.empty()) {
      ctx->addVar(c.var, newTc->getVar(varIdx));
      ctx->getBase()->addVar(newTc->getVar(varIdx));
    }

    ctx->addBlock(cBlock);
    transform(c.suite);
    condSetTerminator(Ns<JumpTerminator>(stmt->getSrcInfo(), doneBlock));

    ctx->popBlock();
    ctx->removeLevel();
  }
  if (stmt->finally) {
    // Each finally gets its own level
    ctx->addLevel();

    auto fBlock = newBlock();
    ctx->addBlock(fBlock);
    transform(stmt->finally);
    condSetTerminator(Ns<FinallyTerminator>(stmt->getSrcInfo(), newTc));
    ctx->popBlock();

    newTc->setFinallyBlock(fBlock);

    ctx->removeLevel();
  }

  // Replace the current block with doneBlock
  ctx->replaceBlock(doneBlock);
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
    f->realize(realizeType(t->getClass())->as<ir::types::FuncType>(), names);
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
