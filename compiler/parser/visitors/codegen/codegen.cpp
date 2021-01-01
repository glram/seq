#include "util/fmt/format.h"
#include <memory>
#include <sstream>
#include <stack>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "parser/ast.h"
#include "parser/common.h"
#include "parser/visitors/codegen/codegen.h"
#include "parser/visitors/codegen/codegen_ctx.h"

using fmt::format;
using std::function;
using std::get;
using std::make_unique;
using std::move;
using std::stack;
using std::unique_ptr;
using std::vector;

using namespace seq::ir;

namespace seq {
namespace ast {

void CodegenVisitor::defaultVisit(const Expr *n) {
  seqassert(false, "invalid node {}", n->toString());
}

void CodegenVisitor::defaultVisit(const Stmt *n) {
  seqassert(false, "invalid node {}", n->toString());
}

void CodegenVisitor::defaultVisit(const Pattern *n) {
  seqassert(false, "invalid node {}", n->toString());
}

CodegenVisitor::CodegenVisitor(shared_ptr<CodegenContext> ctx)
    : ctx(move(ctx)), result() {}

ValuePtr CodegenVisitor::transform(const ExprPtr &expr) {
  CodegenVisitor v(ctx);
  v.setSrcInfo(expr->getSrcInfo());
  expr->accept(v);
  return move(v.result);
}

seq::ir::types::Type *CodegenVisitor::realizeType(types::ClassTypePtr t) {
  auto i = ctx->types.find(t->getClass()->realizeString());
  assert(i != ctx->types.end());
  return i->second;
}

ValuePtr CodegenVisitor::transform(const StmtPtr &stmt) {
  CodegenVisitor v(ctx);
  v.setSrcInfo(stmt->getSrcInfo());
  stmt->accept(v);
  return move(v.result);
}

IRModulePtr CodegenVisitor::apply(shared_ptr<Cache> cache, StmtPtr stmts) {
  auto *module = new IRModule("module");
  auto *main = module->Nr<ir::BodiedFunc>(module->getVoidRetAndArgFuncType(), "main");
  module->setMainFunc(FuncPtr(main));

  auto *block = module->Nr<SeriesFlow>("body");
  main->setBody(FlowPtr(block));

  auto ctx = make_shared<CodegenContext>(cache, block, main);

  // Now add all realization stubs
  for (auto &ff : cache->classes)
    for (auto &f : ff.second.realizations) {
      auto t = ctx->realizeType(f.second.type->getClass());
      ctx->addType(f.first, t);
    }
  for (auto &ff : cache->functions)
    for (auto &f : ff.second.realizations) {
      auto t = f.second.type;
      assert(t);
      auto ast = cache->functions[ff.first].ast.get();
      if (in(ast->attributes, ATTR_INTERNAL)) {
        vector<ir::types::Type *> types;
        auto p = t->parent;
        assert(in(ast->attributes, ATTR_PARENT_CLASS));
        if (!in(ast->attributes, ATTR_NOT_STATIC)) { // hack for non-generic types
          for (auto &x :
               ctx->cache->classes[ast->attributes[ATTR_PARENT_CLASS]].realizations) {
            if (startswith(t->realizeString(), x.first)) {
              p = x.second.type;
              break;
            }
          }
        }
        seqassert(p && p->getClass(), "parent must be set ({}) for {}; parent={}",
                  p ? p->toString() : "-", t->toString(),
                  ast->attributes[ATTR_PARENT_CLASS]);
        ir::types::Type *typ = ctx->realizeType(p->getClass()->getClass());
        int startI = 1;
        if (!ast->args.empty() && ast->args[0].name == "self")
          startI = 2;
        for (int i = startI; i < t->args.size(); i++)
          types.push_back(ctx->realizeType(t->args[i]->getClass()));

        auto names = split(ast->name, '.');
        auto name = names.back();
        if (std::isdigit(name[0])) // TODO: get rid of this hack
          name = names[names.size() - 2];
        LOG_REALIZE("[codegen] generating internal fn {} -> {}", ast->name, name);
        auto fn = module->Nr<seq::ir::InternalFunc>(module->getVoidRetAndArgFuncType(),
                                                    ast->name);
        fn->setParentType(typ);
        ctx->functions[f.first] = {fn, false};
        module->push_back(VarPtr(fn));
      } else if (in(ast->attributes, "llvm")) {
        auto fn = module->Nr<seq::ir::LLVMFunc>(module->getVoidRetAndArgFuncType(),
                                                ast->name);
        ctx->functions[f.first] = {fn, false};
        module->push_back(VarPtr(fn));
      } else if (in(ast->attributes, ".c")) {
        auto fn = module->Nr<seq::ir::ExternalFunc>(module->getVoidRetAndArgFuncType(),
                                                    ast->name);
        ctx->functions[f.first] = {fn, false};
        module->push_back(VarPtr(fn));
      } else {
        auto fn = module->Nr<seq::ir::BodiedFunc>(module->getVoidRetAndArgFuncType(),
                                                  ast->name);
        ctx->functions[f.first] = {fn, false};

        if (in(ast->attributes, "builtin")) {
          fn->setBuiltin();
        }

        module->push_back(VarPtr(fn));
      }
      ctx->addFunc(f.first, ctx->functions[f.first].first);
    }

  CodegenVisitor(ctx).transform(stmts);

  return IRModulePtr(module);
}

void CodegenVisitor::visit(const BoolExpr *expr) {
  result = ctx->getModule()->Nxs<BoolConstant>(
      expr, expr->value, ctx->realizeType(expr->getType()->getClass()));
}

void CodegenVisitor::visit(const IntExpr *expr) {
  result = ctx->getModule()->Nxs<IntConstant>(
      expr, expr->intValue, ctx->realizeType(expr->getType()->getClass()));
}

void CodegenVisitor::visit(const FloatExpr *expr) {
  result = ctx->getModule()->Nxs<FloatConstant>(
      expr, expr->value, ctx->realizeType(expr->getType()->getClass()));
}

void CodegenVisitor::visit(const StringExpr *expr) {
  result = ctx->getModule()->Nxs<StringConstant>(
      expr, expr->value, ctx->realizeType(expr->getType()->getClass()));
}

void CodegenVisitor::visit(const IdExpr *expr) {
  auto *module = ctx->getModule();

  auto val = ctx->find(expr->value);
  seqassert(val, "cannot find '{}'", expr->value);
  // TODO: this makes no sense: why setAtomic on temporary expr?
  // if (var->isGlobal() && var->getBase() == ctx->getBase() &&
  //     ctx->hasFlag("atomic"))
  //   dynamic_cast<seq::VarExpr *>(i->getExpr())->setAtomic();

  if (auto *v = val->getVar())
    result = module->Nxs<VarValue>(expr, v);
  else if (auto *f = val->getFunc())
    result = module->Nxs<VarValue>(expr, f);
  else
    assert(false);
}

void CodegenVisitor::visit(const IfExpr *expr) {
  result = ctx->getModule()->Nxs<TernaryInstr>(
      expr, transform(expr->cond), transform(expr->ifexpr), transform(expr->elsexpr));
}

void CodegenVisitor::visit(const CallExpr *expr) {
  auto lhs = transform(expr->expr);
  vector<ValuePtr> items;
  for (auto &&i : expr->args) {
    if (CAST(i.value, EllipsisExpr))
      assert(false);
    else
      items.push_back(transform(i.value));
  }
  result = ctx->getModule()->Nxs<CallInstr>(expr, transform(expr->expr), move(items));
}

void CodegenVisitor::visit(const StackAllocExpr *expr) {
  auto c = expr->typeExpr->getType()->getClass();
  assert(c);
  result = ctx->getModule()->Nxs<StackAllocInstr>(
      expr, ctx->realizeType(expr->getType()->getClass()), transform(expr->expr));
}

void CodegenVisitor::visit(const DotExpr *expr) {
  auto *module = ctx->getModule();
  result = module->Nxs<ExtractInstr>(expr, transform(expr->expr), expr->member);
}

void CodegenVisitor::visit(const PtrExpr *expr) {
  auto i = CAST(expr->expr, IdExpr);
  assert(i);
  auto var = i->value;
  auto val = ctx->find(var, true);
  assert(val && val->getVar());

  result = ctx->getModule()->Nxs<PointerValue>(expr, val->getVar());
}

void CodegenVisitor::visit(const YieldExpr *expr) {
  result = ctx->getModule()->Nxs<YieldInInstr>(
      expr, ctx->realizeType(expr->getType()->getClass()));
}

void CodegenVisitor::visit(const StmtExpr *expr) {
  ctx->addScope();

  auto bodySeries = newScope(expr, "body");
  ctx->addSeries(bodySeries.get());
  for (auto &s : expr->stmts) {
    transform(s);
  }
  ctx->popSeries();
  result =
      ctx->getModule()->Nxs<FlowInstr>(expr, move(bodySeries), transform(expr->expr));

  ctx->popScope();
}

void CodegenVisitor::visit(const SuiteStmt *stmt) {
  for (auto &s : stmt->stmts)
    transform(s);
}

void CodegenVisitor::visit(const PassStmt *stmt) {}

void CodegenVisitor::visit(const BreakStmt *stmt) {
  auto *module = ctx->getModule();
  ctx->getSeries()->push_back(
      module->Nxs<BreakInstr>(stmt, module->Nxs<ValueProxy>(stmt, ctx->getLoop())));
}

void CodegenVisitor::visit(const ContinueStmt *stmt) {
  auto *module = ctx->getModule();
  ctx->getSeries()->push_back(
      module->Nxs<ContinueInstr>(stmt, module->Nxs<ValueProxy>(stmt, ctx->getLoop())));
}

void CodegenVisitor::visit(const ExprStmt *stmt) {
  ctx->getSeries()->push_back(transform(stmt->expr));
}

void CodegenVisitor::visit(const AssignStmt *stmt) {
  /// TODO: atomic operations & JIT
  auto i = CAST(stmt->lhs, IdExpr);
  assert(i);
  auto var = i->value;

  auto *module = ctx->getModule();

  if (!stmt->rhs) {
    if (var == ".__argv__") {
      if (!module->getArgVar())
        module->setArgVar(
            module->Nx<ir::Var>(module->getArrayType(module->getStringType()), "argv"));
      ctx->addVar(var, module->getArgVar().get());
    } else {
      auto *newVar = module->Nrs<ir::Var>(
          stmt, ctx->realizeType(stmt->lhs->getType()->getClass()), var);
      if (in(ctx->cache->globals, var)) {
        ctx->getModule()->push_back(wrap(newVar));
      } else {
        ctx->getBase()->push_back(wrap(newVar));
      }
      ctx->addVar(var, newVar, in(ctx->cache->globals, var));
    }
  } else if (stmt->rhs->isType()) {
    // ctx->addType(var, realizeType(stmt->rhs->getType()->getClass()));
  } else {
    auto *newVar = module->Nrs<ir::Var>(
        stmt, ctx->realizeType(stmt->rhs->getType()->getClass()), var);
    if (in(ctx->cache->globals, var)) {
      ctx->getModule()->push_back(wrap(newVar));
    } else {
      ctx->getBase()->push_back(wrap(newVar));
    }
    ctx->addVar(var, newVar, var[0] == '.');
    ctx->getSeries()->push_back(
        module->Nxs<AssignInstr>(stmt, newVar, transform(stmt->rhs)));
  }
}

void CodegenVisitor::visit(const AssignMemberStmt *stmt) {
  auto *module = ctx->getModule();
  ctx->getSeries()->push_back(module->Nxs<InsertInstr>(
      stmt, transform(stmt->lhs), stmt->member, transform(stmt->rhs)));
}

void CodegenVisitor::visit(const UpdateStmt *stmt) {
  auto i = CAST(stmt->lhs, IdExpr);
  assert(i);
  auto var = i->value;
  auto val = ctx->find(var, true);
  assert(val && val->getVar());

  auto *module = ctx->getModule();
  ctx->getSeries()->push_back(
      module->Nxs<AssignInstr>(stmt, val->getVar(), transform(stmt->rhs)));
}

void CodegenVisitor::visit(const DelStmt *stmt) {
  auto expr = CAST(stmt->expr, IdExpr);
  assert(expr);
  auto v = ctx->find(expr->value, true)->getVar();
  assert(v);
  ctx->remove(expr->value);
}

void CodegenVisitor::visit(const ReturnStmt *stmt) {
  auto *module = ctx->getModule();
  ValuePtr value;
  if (stmt->expr)
    value = transform(stmt->expr);

  ctx->getSeries()->push_back(module->Nxs<ReturnInstr>(stmt, move(value)));
}

void CodegenVisitor::visit(const YieldStmt *stmt) {
  auto *module = ctx->getModule();
  ValuePtr value;
  if (stmt->expr)
    value = transform(stmt->expr);

  ctx->getSeries()->push_back(module->Nxs<YieldInstr>(stmt, move(value)));
}

void CodegenVisitor::visit(const WhileStmt *stmt) {
  auto loop = ctx->getModule()->Nxs<WhileFlow>(stmt, transform(stmt->cond),
                                               newScope(stmt, "body"));

  ctx->addLoop(loop.get());
  ctx->addScope();
  ctx->addSeries(cast<SeriesFlow>(loop->getBody()));
  transform(stmt->suite);
  ctx->popSeries();
  ctx->popScope();
  ctx->popLoop();

  ctx->getSeries()->push_back(move(loop));
}

void CodegenVisitor::visit(const ForStmt *stmt) {
  auto *module = ctx->getModule();

  auto varId = CAST(stmt->var, IdExpr);
  auto *resVar = module->Nrs<ir::Var>(
      stmt, ctx->realizeType(varId->getType()->getClass()), varId->value);
  ctx->getBase()->push_back(wrap(resVar));

  auto bodySeries = newScope(stmt, "body");
  auto loop = ctx->getModule()->Nxs<ForFlow>(stmt, transform(stmt->iter),
                                             move(bodySeries), resVar);
  ctx->addLoop(loop.get());
  ctx->addScope();
  ctx->addVar(varId->value, resVar);

  ctx->addSeries(cast<SeriesFlow>(loop->getBody()));
  transform(stmt->suite);
  ctx->popSeries();
  ctx->popScope();
  ctx->popLoop();

  ctx->getSeries()->push_back(move(loop));
}

void CodegenVisitor::visit(const IfStmt *stmt) {
  auto trueSeries = newScope(stmt, "ifstmt_true");
  ctx->addScope();
  ctx->addSeries(trueSeries.get());
  transform(stmt->ifs[0].suite);
  ctx->popSeries();
  ctx->popScope();

  unique_ptr<SeriesFlow> falseSeries;
  if (stmt->ifs.size() > 1) {
    falseSeries = newScope(stmt, "ifstmt_false");
    ctx->addScope();
    ctx->addSeries(falseSeries.get());
    transform(stmt->ifs[1].suite);
    ctx->popSeries();
    ctx->popScope();
  }

  ctx->getSeries()->push_back(ctx->getModule()->Nxs<IfFlow>(
      stmt, transform(stmt->ifs[0].cond), move(trueSeries), move(falseSeries)));
}

void CodegenVisitor::visit(const TryStmt *stmt) {
  auto bodySeries = newScope(stmt, "body");
  ctx->addScope();
  ctx->addSeries(bodySeries.get());
  transform(stmt->suite);
  ctx->popSeries();
  ctx->popScope();

  unique_ptr<SeriesFlow> finallySeries;
  if (stmt->finally) {
    finallySeries = newScope(stmt, "finally");
    ctx->addScope();
    ctx->addSeries(finallySeries.get());
    transform(stmt->finally);
    ctx->popSeries();
    ctx->popScope();
  }

  auto newTc = Nx<TryCatchFlow>(stmt, move(bodySeries), move(finallySeries));

  for (auto &c : stmt->catches) {
    auto catchBody = newScope(stmt, "catch");
    auto *excType = c.exc ? ctx->realizeType(c.exc->getType()->getClass()) : nullptr;

    ctx->addScope();

    ir::Var *catchVar = nullptr;
    if (!c.var.empty()) {
      catchVar = ctx->getModule()->Nrs<ir::Var>(stmt, excType, c.var);
      ctx->addVar(c.var, catchVar);
      ctx->getBase()->push_back(wrap(catchVar));
    }

    ctx->addSeries(catchBody.get());
    transform(c.suite);
    ctx->popSeries();

    ctx->popScope();

    newTc->push_back(TryCatchFlow::Catch(move(catchBody), excType, catchVar));
  }

  ctx->getSeries()->push_back(move(newTc));
}

void CodegenVisitor::visit(const ThrowStmt *stmt) {
  ctx->getSeries()->push_back(Nx<ThrowInstr>(stmt, transform(stmt->expr)));
}

void CodegenVisitor::visit(const FunctionStmt *stmt) {
  for (auto &real : ctx->cache->functions[stmt->name].realizations) {
    auto &fp = ctx->functions[real.first];
    if (fp.second)
      continue;
    fp.second = true;

    const auto &ast = real.second.ast;
    assert(ast);

    vector<string> names;
    vector<seq::ir::types::Type *> types;
    auto t = real.second.type;
    for (int i = 1; i < t->args.size(); i++) {
      types.push_back(ctx->realizeType(t->args[i]->getClass()));
      names.push_back(ast->args[i - 1].name);
    }

    LOG_REALIZE("[codegen] generating fn {}", real.first);
    if (in(stmt->attributes, "llvm")) {
      auto *f = cast<ir::LLVMFunc>(fp.first);
      assert(f);
      f->realize(cast<ir::types::FuncType>(ctx->realizeType(t->getClass())), names);

      // auto s = CAST(tmp->suite, SuiteStmt);
      // assert(s && s->stmts.size() == 1)
      auto c = ast->suite->firstInBlock();
      assert(c);
      auto e = c->getExpr();
      assert(e);
      auto sp = CAST(e->expr, StringExpr);
      assert(sp);

      std::vector<ir::LLVMFunc::LLVMLiteral> literals;
      auto &ss = ast->suite->getSuite()->stmts;
      for (int i = 1; i < ss.size(); i++) {
        auto &ex = ss[i]->getExpr()->expr;
        if (auto ei = ex->getInt()) { // static expr
          literals.emplace_back(ei->intValue);
        } else {
          seqassert(ex->isType() && ex->getType(), "invalid LLVM type argument");
          literals.emplace_back(ctx->realizeType(ex->getType()->getClass()));
        }
      }

      std::istringstream sin(sp->value);
      string l, declare, code;
      bool isDeclare = true;
      vector<string> lines;
      while (std::getline(sin, l)) {
        string lp = l;
        ltrim(lp);
        rtrim(lp);
        if (isDeclare && !startswith(lp, "declare ")) {
          bool isConst = lp.find("private constant") != string::npos;
          if (!isConst) {
            isDeclare = false;
            if (!lp.empty() && lp.back() != ':')
              lines.push_back("entry:");
          }
        }
        if (isDeclare)
          declare += lp + "\n";
        else
          lines.push_back(l);
      }
      f->setLLVMBody(join(lines, "\n"));
      f->setLLVMDeclarations(move(declare));
      f->setLLVMLiterals(move(literals));
    } else {
      auto *f = cast<ir::Func>(fp.first);
      assert(f);
      f->setSrcInfo(getSrcInfo());
      //      if (!ctx->isToplevel())
      //        f->p
      ctx->addScope();

      f->realize(cast<ir::types::FuncType>(ctx->realizeType(t->getClass())), names);
      f->setAttribute(kFuncAttribute, make_unique<FuncAttribute>(ast->attributes));
      for (auto &a : ast->attributes) {
        if (a.first == "atomic")
          ctx->setFlag("atomic");
      }
      if (in(ast->attributes, ".c")) {
        auto *external = cast<ir::ExternalFunc>(f);
        assert(external);
        external->setUnmangledName(ctx->cache->reverseIdentifierLookup[stmt->name]);
      } else if (!in(ast->attributes, "internal")) {
        for (auto &arg : names) {
          auto var = cast<ir::Var>(f->getArgVar(arg));
          assert(var);
          ctx->addVar(arg, var);
        }
        auto body = newScope(stmt, "body");
        ctx->addSeries(body.get(), f);
        transform(ast->suite);
        ctx->popSeries();

        auto *bodied = cast<ir::BodiedFunc>(f);
        assert(bodied);

        bodied->setBody(move(body));
      }
      ctx->popScope();
    }
  }
} // namespace tmp

void CodegenVisitor::visit(const ClassStmt *stmt) {
  // visitMethods(ctx->getRealizations()->getCanonicalName(stmt->getSrcInfo()));
}

std::unique_ptr<ir::SeriesFlow> CodegenVisitor::newScope(const seq::SrcObject *s,
                                                         std::string name) {
  return ctx->getModule()->Nxs<SeriesFlow>(s, std::move(name));
}

} // namespace ast
} // namespace seq
