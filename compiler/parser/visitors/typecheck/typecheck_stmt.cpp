/*
 * typecheck_stmt.cpp --- Type inference for AST statements.
 *
 * (c) Seq project. All rights reserved.
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 */

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "parser/ast.h"
#include "parser/common.h"
#include "parser/visitors/simplify/simplify_ctx.h"
#include "parser/visitors/typecheck/typecheck.h"
#include "parser/visitors/typecheck/typecheck_ctx.h"

using fmt::format;
using std::deque;
using std::dynamic_pointer_cast;
using std::get;
using std::move;
using std::ostream;
using std::stack;
using std::static_pointer_cast;

namespace seq {
namespace ast {

using namespace types;

StmtPtr TypecheckVisitor::transform(const StmtPtr &stmt_) {
  auto &stmt = const_cast<StmtPtr &>(stmt_);
  if (!stmt || stmt->done)
    return move(stmt);
  TypecheckVisitor v(ctx);
  v.setSrcInfo(stmt->getSrcInfo());
  stmt->accept(v);
  if (v.resultStmt)
    stmt = move(v.resultStmt);
  if (!v.prependStmts->empty()) {
    if (stmt)
      v.prependStmts->push_back(move(stmt));
    bool done = true;
    for (auto &s : *(v.prependStmts))
      done &= s->done;
    stmt = N<SuiteStmt>(move(*v.prependStmts));
    stmt->done = done;
  }
  return move(stmt);
}

void TypecheckVisitor::defaultVisit(Stmt *s) {
  seqassert(false, "unexpected AST node {}", s->toString());
}

/**************************************************************************************/

void TypecheckVisitor::visit(SuiteStmt *stmt) {
  vector<StmtPtr> stmts;
  stmt->done = true;
  for (auto &s : stmt->stmts)
    if (auto t = transform(s)) {
      stmts.push_back(move(t));
      stmt->done &= stmts.back()->done;
    }
  stmt->stmts = move(stmts);
}

void TypecheckVisitor::visit(PassStmt *stmt) { stmt->done = true; }

void TypecheckVisitor::visit(BreakStmt *stmt) { stmt->done = true; }

void TypecheckVisitor::visit(ContinueStmt *stmt) { stmt->done = true; }

void TypecheckVisitor::visit(ExprStmt *stmt) {
  stmt->expr = transform(stmt->expr);
  stmt->done = stmt->expr->done;
}

void TypecheckVisitor::visit(AssignStmt *stmt) {
  // Simplify stage ensures that lhs is always IdExpr.
  string lhs;
  if (auto e = stmt->lhs->getId())
    lhs = e->value;
  seqassert(!lhs.empty(), "invalid AssignStmt {}", stmt->lhs->toString());
  stmt->rhs = transform(stmt->rhs);
  stmt->type = transformType(stmt->type);
  TypecheckItem::Kind kind;
  if (!stmt->rhs) { // Case 1: forward declaration: x: type
    stmt->lhs->type |= stmt->type ? stmt->type->getType()
                                  : ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel);
    ctx->add(kind = TypecheckItem::Var, lhs, stmt->lhs->type);
    stmt->done = realizeType(stmt->lhs->type) != nullptr;
  } else { // Case 2: Normal assignment
    if (stmt->type && stmt->type->getType()->getClass()) {
      stmt->lhs->type |= ctx->instantiate(getSrcInfo(), stmt->type->getType());
      wrapOptionalIfNeeded(stmt->lhs->getType(), stmt->rhs);
      stmt->lhs->type |= stmt->rhs->type;
    }
    kind = stmt->rhs->isType() ? TypecheckItem::Type
                               : (stmt->rhs->getType()->getFunc() ? TypecheckItem::Func
                                                                  : TypecheckItem::Var);
    ctx->add(kind, lhs, stmt->rhs->getType());
    stmt->done = stmt->rhs->done;
  }
  // Save the variable to the local realization context
  ctx->bases.back().visitedAsts[lhs] = {kind, stmt->lhs->type};
}

void TypecheckVisitor::visit(UpdateStmt *stmt) {
  stmt->lhs = transform(stmt->lhs);

  // Case 1: Check for atomic and in-place updates (a += b).
  // In-place updates (a += b) are stored as Update(a, Binary(a + b, inPlace=true)).
  auto b = const_cast<BinaryExpr *>(stmt->rhs->getBinary());
  if (b && b->inPlace) {
    bool noReturn = false;
    auto oldRhsType = stmt->rhs->type;
    if (auto nb = transformBinary(b, stmt->isAtomic, &noReturn))
      stmt->rhs = move(nb);
    oldRhsType |= stmt->rhs->type;
    if (stmt->rhs->getBinary()) { // still BinaryExpr: will be transformed later.
      stmt->lhs->type |= stmt->rhs->type;
      return;
    } else if (noReturn) { // remove assignment, call update function instead
                           // (__i***__ or __atomic_***__)
      bool done = stmt->rhs->done;
      resultStmt = N<ExprStmt>(move(stmt->rhs));
      resultStmt->done = done;
      return;
    }
  }
  // Case 2: Check for atomic min and max operations: a = min(a, ...).
  // NOTE: does not check for a = min(..., a).
  auto lhsClass = stmt->lhs->getType()->getClass();
  CallExpr *c;
  if (stmt->isAtomic && stmt->lhs->getId() &&
      (c = const_cast<CallExpr *>(stmt->rhs->getCall())) &&
      (c->expr->isId("min") || c->expr->isId("max")) && c->args.size() == 2 &&
      c->args[0].value->isId(string(stmt->lhs->getId()->value))) {
    auto ptrTyp =
        ctx->instantiateGeneric(getSrcInfo(), ctx->findInternal("Ptr"), {lhsClass});
    c->args[1].value = transform(c->args[1].value);
    auto rhsTyp = c->args[1].value->getType()->getClass();
    if (auto method = findBestMethod(lhsClass.get(),
                                     format("__atomic_{}__", c->expr->getId()->value),
                                     {{"", ptrTyp}, {"", rhsTyp}})) {
      resultStmt = transform(
          N<ExprStmt>(N<CallExpr>(N<IdExpr>(method->name), N<PtrExpr>(move(stmt->lhs)),
                                  move(c->args[1].value))));
      return;
    }
  }

  stmt->rhs = transform(stmt->rhs);
  auto rhsClass = stmt->rhs->getType()->getClass();
  // Case 3: check for an atomic assignment.
  if (stmt->isAtomic && lhsClass && rhsClass) {
    auto ptrType =
        ctx->instantiateGeneric(getSrcInfo(), ctx->findInternal("Ptr"), {lhsClass});
    if (auto m = findBestMethod(lhsClass.get(), "__atomic_xchg__",
                                {{"", ptrType}, {"", rhsClass}})) {
      resultStmt = transform(N<ExprStmt>(N<CallExpr>(
          N<IdExpr>(m->name), N<PtrExpr>(move(stmt->lhs)), move(stmt->rhs))));
      return;
    }
    stmt->isAtomic = false;
  }
  // Case 4: handle optionals if needed.
  wrapOptionalIfNeeded(stmt->lhs->getType(), stmt->rhs);
  stmt->lhs->type |= stmt->rhs->type;
  stmt->done = stmt->rhs->done;
}

void TypecheckVisitor::visit(AssignMemberStmt *stmt) {
  stmt->lhs = transform(stmt->lhs);
  stmt->rhs = transform(stmt->rhs);
  auto lhsClass = stmt->lhs->getType()->getClass();

  if (lhsClass) {
    auto member = ctx->findMember(lhsClass->name, stmt->member);
    if (!member && lhsClass->name == "Optional") {
      // Unwrap optional and look up there:
      resultStmt = transform(
          N<AssignMemberStmt>(N<CallExpr>(N<IdExpr>("unwrap"), move(stmt->lhs)),
                              stmt->member, move(stmt->rhs)));
      return;
    }
    if (!member)
      error("cannot find '{}' in {}", stmt->member, lhsClass->name);
    if (lhsClass->isRecord())
      error("tuple element {} is read-only", stmt->member);
    auto typ = ctx->instantiate(getSrcInfo(), member, lhsClass.get());
    wrapOptionalIfNeeded(typ, stmt->rhs);
    stmt->rhs->type |= typ;
    stmt->done = stmt->rhs->done;
  }
}

void TypecheckVisitor::visit(ReturnStmt *stmt) {
  stmt->expr = transform(stmt->expr);
  if (stmt->expr) {
    auto &base = ctx->bases.back();
    wrapOptionalIfNeeded(base.returnType, stmt->expr);
    base.returnType |= stmt->expr->type;
    // HACK TODO: elide "return void" in Partial.__call__
    auto retTyp = stmt->expr->getType()->getClass();
    if (startswith(base.name, "Partial.N") && endswith(base.name, ".__call__") &&
        retTyp && retTyp->name == "void") {
      resultStmt = transform(N<ExprStmt>(move(stmt->expr)));
      return;
    }
    stmt->done = stmt->expr->done;
  } else {
    stmt->done = true;
  }
}

void TypecheckVisitor::visit(YieldStmt *stmt) {
  if (stmt->expr)
    stmt->expr = transform(stmt->expr);
  auto baseTyp = stmt->expr ? stmt->expr->getType() : ctx->findInternal("void");
  ctx->bases.back().returnType |= ctx->instantiateGeneric(
      stmt->getSrcInfo(), ctx->findInternal("Generator"), {baseTyp});
  stmt->done = stmt->expr ? stmt->expr->done : true;
}

void TypecheckVisitor::visit(WhileStmt *stmt) {
  stmt->cond = transform(stmt->cond);
  stmt->suite = transform(stmt->suite);
  stmt->done = stmt->cond->done && stmt->suite->done;
}

void TypecheckVisitor::visit(ForStmt *stmt) {
  stmt->iter = transform(stmt->iter);
  // Extract the type of the for variable.
  TypePtr varType = ctx->addUnbound(stmt->var->getSrcInfo(), ctx->typecheckLevel);
  if (!stmt->iter->getType()->getUnbound()) {
    auto iterType = stmt->iter->getType()->getClass();
    if (!iterType || iterType->name != "Generator")
      error(stmt->iter, "for loop expected a generator");
    varType |= iterType->explicits[0].type;
  }
  string varName;
  if (auto e = stmt->var->getId())
    varName = e->value;
  seqassert(!varName.empty(), "invalid for variable {}", stmt->var->toString());
  stmt->var->type |= varType;
  ctx->add(TypecheckItem::Var, varName, varType);
  stmt->suite = transform(stmt->suite);
  stmt->done = stmt->iter->done && stmt->suite->done;
}

void TypecheckVisitor::visit(IfStmt *stmt) {
  vector<IfStmt::If> ifs;
  stmt->done = true;
  for (auto &i : stmt->ifs) {
    if ((i.cond = transform(i.cond)))
      stmt->done &= i.cond->done;
    if ((i.suite = transform(i.suite)))
      stmt->done &= i.suite->done;
  }
}

void TypecheckVisitor::visit(MatchStmt *stmt) {
  stmt->what = transform(stmt->what);
  auto matchType = stmt->what->getType();
  auto matchTypeClass = matchType->getClass();

  // TODO: hack for seq/K-mer unification
  auto unifyMatchType = [&](const TypePtr &t) {
    seqassert(t && matchType, "invalid match type");
    types::Type::Unification undo;
    undo.isMatch = true;
    if (t->unify(matchType.get(), &undo) < 0) {
      undo.undo();
      error("cannot unify {} and {}", t->toString(), matchType->toString());
    }
  };

  stmt->done = true;
  for (auto ci = 0; ci < stmt->cases.size(); ci++) {
    if (auto p = CAST(stmt->patterns[ci], BoundPattern)) {
      p->pattern = transform(p->pattern);
      ctx->add(TypecheckItem::Var, p->var, p->pattern->getType());
      unifyMatchType(p->pattern->getType());
      stmt->done &= p->pattern->done;
    } else {
      stmt->patterns[ci] = transform(stmt->patterns[ci]);
      unifyMatchType(stmt->patterns[ci]->getType());
      stmt->done &= stmt->patterns[ci]->done;
    }
    stmt->cases[ci] = transform(stmt->cases[ci]);
    stmt->done &= stmt->cases[ci]->done;
  }
}

void TypecheckVisitor::visit(TryStmt *stmt) {
  vector<TryStmt::Catch> catches;
  stmt->suite = transform(stmt->suite);
  stmt->done &= stmt->suite->done;
  for (auto &c : stmt->catches) {
    c.exc = transformType(c.exc);
    if (!c.var.empty())
      ctx->add(TypecheckItem::Var, c.var, c.exc->getType());
    c.suite = transform(c.suite);
    stmt->done &= (c.exc ? c.exc->done : true) && c.suite->done;
  }
  stmt->finally = transform(stmt->finally);
  stmt->done &= stmt->finally->done;
}

void TypecheckVisitor::visit(ThrowStmt *stmt) {
  stmt->expr = transform(stmt->expr);
  stmt->done &= stmt->expr->done;
}

void TypecheckVisitor::visit(FunctionStmt *stmt) {
  if (auto t = ctx->findInVisited(stmt->name).second) {
    // We realize built-ins and extern C function when we see them for the second time
    // (to avoid preamble realization).
    if (in(stmt->attributes, ATTR_BUILTIN) || in(stmt->attributes, ATTR_EXTERN_C)) {
      if (!t->canRealize())
        error("builtins and external functions must be realizable");
      auto typ = ctx->instantiate(getSrcInfo(), t);
      typ |= realizeFunc(typ->getFunc());
    }
    stmt->done = true;
    return;
  }

  // Parse preamble.
  auto &attributes = const_cast<FunctionStmt *>(stmt)->attributes;
  bool isClassMember = in(stmt->attributes, ATTR_PARENT_CLASS);
  auto explicits = parseGenerics(stmt->generics, ctx->typecheckLevel); // level down
  vector<TypePtr> generics;
  if (isClassMember && in(attributes, ATTR_NOT_STATIC)) {
    // Fetch parent class generics.
    auto parentClassAST = ctx->cache->classes[attributes[ATTR_PARENT_CLASS]].ast.get();
    auto parentClass = ctx->find(attributes[ATTR_PARENT_CLASS])->type->getClass();
    seqassert(parentClass, "parent class not set");
    for (int i = 0; i < parentClassAST->generics.size(); i++) {
      auto gen = parentClass->explicits[i].type->getLink();
      generics.push_back(
          make_shared<LinkType>(LinkType::Unbound, parentClass->explicits[i].id,
                                ctx->typecheckLevel - 1, nullptr, gen->isStatic));
      ctx->add(TypecheckItem::Type, parentClassAST->generics[i].name, generics.back(),
               gen->isStatic);
    }
  }
  for (const auto &i : stmt->generics)
    generics.push_back(ctx->find(i.name)->type);
  // Add function arguments.
  vector<TypePtr> args;
  {
    ctx->typecheckLevel++;
    if (stmt->ret) {
      args.push_back(transformType(stmt->ret)->getType());
    } else {
      args.push_back(ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel));
      generics.push_back(args.back());
    }
    for (auto &a : stmt->args) {
      if (!a.type) {
        args.push_back(ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel));
        generics.push_back(args.back());
      } else {
        args.push_back(transformType(a.type)->getType());
      }
      ctx->add(TypecheckItem::Var, a.name, args.back());
    }
    ctx->typecheckLevel--;
  }
  // Generalize generics.
  for (auto &g : generics) {
    seqassert(g && g->getLink() && g->getLink()->kind != types::LinkType::Link,
              "generic has been unified");
    if (g->getLink()->kind == LinkType::Unbound)
      g->getLink()->kind = LinkType::Generic;
  }
  // Construct the type.
  auto typ = make_shared<FuncType>(
      stmt->name,
      ctx->findInternal(format("Function.N{}", stmt->args.size()))->getClass().get(),
      args, explicits);
  if (stmt->name == "_validate_str_as_seq.ensure_valid")
    assert(1);
  if (isClassMember && in(attributes, ATTR_NOT_STATIC))
    typ->parent = ctx->find(attributes[ATTR_PARENT_CLASS])->type;
  else if (in(attributes, ATTR_PARENT_FUNCTION))
    typ->parent = ctx->bases[ctx->findBase(attributes[ATTR_PARENT_FUNCTION])].type;
  typ->setSrcInfo(stmt->getSrcInfo());
  typ = std::static_pointer_cast<FuncType>(typ->generalize(ctx->typecheckLevel));
  // Check if this is a class method; if so, update the class method lookup table.
  if (isClassMember) {
    auto &methods = ctx->cache->classes[attributes[ATTR_PARENT_CLASS]]
                        .methods[ctx->cache->reverseIdentifierLookup[stmt->name]];
    bool found = false;
    for (auto &i : methods) {
      if (i.name == stmt->name) {
        i.type = typ;
        found = true;
        break;
      }
    }
    seqassert(found, "cannot find matching class method for {}", stmt->name);
  }
  // Update visited table.
  ctx->bases[ctx->findBase(attributes[ATTR_PARENT_FUNCTION])]
      .visitedAsts[stmt->name] = {TypecheckItem::Func, typ};
  ctx->add(TypecheckItem::Func, stmt->name, typ);

  LOG_REALIZE("[stmt] added func {}: {} (base={})", stmt->name, typ->toString(),
              ctx->getBase());
}

void TypecheckVisitor::visit(ClassStmt *stmt) {
  if (ctx->findInVisited(stmt->name).second && !in(stmt->attributes, "extend"))
    return;

  auto &attributes = const_cast<ClassStmt *>(stmt)->attributes;
  bool extension = in(attributes, "extend");
  ClassTypePtr typ = nullptr;
  if (!extension) {
    typ = make_shared<ClassType>(stmt->name, stmt->isRecord(), vector<TypePtr>(),
                                 vector<Generic>(), nullptr);
    if (in(stmt->attributes, ATTR_TRAIT))
      typ->isTrait = true;
    typ->setSrcInfo(stmt->getSrcInfo());
    ctx->add(TypecheckItem::Type, stmt->name, typ);
    ctx->bases[ctx->findBase(attributes[ATTR_PARENT_FUNCTION])]
        .visitedAsts[stmt->name] = {TypecheckItem::Type, typ};

    // Parse class fields.
    typ->explicits = parseGenerics(stmt->generics, ctx->typecheckLevel);
    {
      ctx->typecheckLevel++;
      for (auto ai = 0; ai < stmt->args.size(); ai++) {
        ctx->cache->classes[stmt->name].fields[ai].type =
            transformType(stmt->args[ai].type)
                ->getType()
                ->generalize(ctx->typecheckLevel - 1);
        if (stmt->isRecord())
          typ->args.push_back(ctx->cache->classes[stmt->name].fields[ai].type);
      }
      ctx->typecheckLevel--;
    }
    // Remove lingering generics.
    for (const auto &g : stmt->generics) {
      auto val = ctx->find(g.name);
      seqassert(val && val->type && val->type->getLink() &&
                    val->type->getLink()->kind != types::LinkType::Link,
                "generic has been unified");
      if (val->type->getLink()->kind == LinkType::Unbound)
        val->type->getLink()->kind = LinkType::Generic;
      ctx->remove(g.name);
    }

    LOG_REALIZE("[class] {} -> {}", stmt->name, typ->toString());
    for (auto &m : ctx->cache->classes[stmt->name].fields)
      LOG_REALIZE("       - member: {}: {}", m.name, m.type->toString());
  } else {
    // Increase the current extension count.
    ctx->extendCount++;
  }
  //  stmt->done = true;
}

/**************************************************************************************/

vector<types::Generic> TypecheckVisitor::parseGenerics(const vector<Param> &generics,
                                                       int level) {
  auto genericTypes = vector<Generic>();
  for (const auto &g : generics) {
    auto typ = ctx->addUnbound(getSrcInfo(), level, true, bool(g.type));
    genericTypes.emplace_back(Generic{g.name, typ->generalize(level),
                                      ctx->cache->unboundCount - 1, clone(g.deflt)});
    LOG_REALIZE("[generic] {} -> {} {}", g.name, typ->toString(), bool(g.type));
    ctx->add(TypecheckItem::Type, g.name, typ, bool(g.type));
  }
  return genericTypes;
}

} // namespace ast
} // namespace seq