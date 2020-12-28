/**
 * TODO : Redo error messages (right now they are awful)
 */

#include "util/fmt/format.h"
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "parser/ast.h"
#include "parser/common.h"
#include "parser/visitors/simplify/simplify.h"
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

string printParents(types::TypePtr t) {
  string s;
  for (auto p = t; p; p = p = t->getClass()->parent) {
    s = t->toString() + ":" + s;
  }
  return ":" + s;
}

TypecheckVisitor::TypecheckVisitor(shared_ptr<TypeContext> ctx,
                                   shared_ptr<vector<StmtPtr>> stmts)
    : ctx(ctx) {
  prependStmts = stmts ? stmts : make_shared<vector<StmtPtr>>();
}

ExprPtr TypecheckVisitor::transform(const ExprPtr &expr) {
  return transform(expr, false);
}

ExprPtr TypecheckVisitor::transform(const ExprPtr &expr, bool allowTypes) {
  if (!expr)
    return nullptr;
  TypecheckVisitor v(ctx, prependStmts);
  v.setSrcInfo(expr->getSrcInfo());
  expr->accept(v);
  if (v.resultExpr && v.resultExpr->getType() && v.resultExpr->getType()->getClass() &&
      v.resultExpr->getType()->getClass()->canRealize())
    realizeType(v.resultExpr->getType()->getClass());
  seqassert(v.resultExpr, "cannot parse {}", expr->toString());
  return move(v.resultExpr);
}

ExprPtr TypecheckVisitor::transformType(const ExprPtr &expr) {
  auto e = transform(expr, true);
  if (e) {
    if (!e->isType())
      error("expected type expression");
    e->setType(ctx->instantiate(expr->getSrcInfo(), e->getType()));
  }
  return e;
}

StmtPtr TypecheckVisitor::transform(const StmtPtr &stmt) {
  if (!stmt)
    return nullptr;
  TypecheckVisitor v(ctx);
  v.setSrcInfo(stmt->getSrcInfo());
  stmt->accept(v);
  if (v.prependStmts->size()) {
    if (v.resultStmt)
      v.prependStmts->push_back(move(v.resultStmt));
    v.resultStmt = N<SuiteStmt>(move(*v.prependStmts));
  }
  return move(v.resultStmt);
}

PatternPtr TypecheckVisitor::transform(const PatternPtr &pat) {
  if (!pat)
    return nullptr;
  TypecheckVisitor v(ctx, prependStmts);
  v.setSrcInfo(pat->getSrcInfo());
  pat->accept(v);
  return move(v.resultPattern);
}

StmtPtr TypecheckVisitor::apply(shared_ptr<Cache> cache, StmtPtr stmts) {
  auto ctx = make_shared<TypeContext>(cache);
  TypecheckVisitor v(ctx);
  return v.realizeBlock(stmts, true);
}

void TypecheckVisitor::defaultVisit(const Expr *e) { resultExpr = e->clone(); }

void TypecheckVisitor::defaultVisit(const Stmt *s) { resultStmt = s->clone(); }

void TypecheckVisitor::defaultVisit(const Pattern *p) { resultPattern = p->clone(); }

/*************************************************************************************/

void TypecheckVisitor::visit(const BoolExpr *expr) {
  resultExpr = expr->clone();
  resultExpr->setType(ctx->findInternal(".bool"));
}

void TypecheckVisitor::visit(const IntExpr *expr) {
  resultExpr = expr->clone();
  resultExpr->setType(ctx->findInternal(".int"));
}

void TypecheckVisitor::visit(const FloatExpr *expr) {
  resultExpr = expr->clone();
  resultExpr->setType(ctx->findInternal(".float"));
}

void TypecheckVisitor::visit(const StringExpr *expr) {
  resultExpr = expr->clone();
  resultExpr->setType(ctx->findInternal(".str"));
}

void TypecheckVisitor::visit(const IdExpr *expr) {
  auto val = ctx->find(expr->value);
  //  if (!val)
  //    ctx->dump();
  seqassert(val, "cannot find '{}'", expr->value);
  if (expr->value == ".Generator")
    assert(1);
  if (val->isStatic()) {
    auto s = val->getType()->getStatic();
    assert(s);
    resultExpr = transform(N<IntExpr>(s->getValue()));
  } else {
    resultExpr = expr->clone();
    TypePtr typ = val->getType();
    if (val->isType())
      resultExpr->markType();
    typ = ctx->instantiate(getSrcInfo(), val->getType());
    resultExpr->setType(forceUnify(resultExpr, typ));
    auto newName = patchIfRealizable(typ, val->isType());
    if (!newName.empty())
      static_cast<IdExpr *>(resultExpr.get())->value = newName;
  }
}

void TypecheckVisitor::visit(const IfExpr *expr) {
  auto e = N<IfExpr>(transform(expr->cond), transform(expr->ifexpr),
                     transform(expr->elsexpr));
  auto ti = e->ifexpr->getType()->getClass();
  auto te = e->elsexpr->getType()->getClass();
  if (ti && te) {
    if (te->name != ti->name) {
      if (ti->name == ".Optional")
        e->elsexpr = transform(N<CallExpr>(N<IdExpr>(".Optional"), move(e->elsexpr)));
      else if (te->name == ".Optional")
        e->ifexpr = transform(N<CallExpr>(N<IdExpr>(".Optional"), move(e->ifexpr)));
    }
    forceUnify(e->ifexpr->getType(), e->elsexpr->getType());
  }
  e->setType(forceUnify(expr, e->ifexpr->getType()));
  resultExpr = move(e);
}

void TypecheckVisitor::visit(const BinaryExpr *expr) {
  auto e = transformBinary(expr->lexpr, expr->rexpr, expr->op, expr->inPlace);
  e->setType(forceUnify(expr, e->getType()));
  resultExpr = move(e);
}

ExprPtr TypecheckVisitor::transformBinary(const ExprPtr &lexpr, const ExprPtr &rexpr,
                                          const string &op, bool inPlace, bool isAtomic,
                                          bool *noReturn) {
  auto magics = unordered_map<string, string>{
      {"+", "add"},     {"-", "sub"},    {"*", "mul"}, {"**", "pow"}, {"/", "truediv"},
      {"//", "div"},    {"@", "matmul"}, {"%", "mod"}, {"<", "lt"},   {"<=", "le"},
      {">", "gt"},      {">=", "ge"},    {"==", "eq"}, {"!=", "ne"},  {"<<", "lshift"},
      {">>", "rshift"}, {"&", "and"},    {"|", "or"},  {"^", "xor"},  {"min", "min"},
      {"max", "max"}};
  if (noReturn)
    *noReturn = false;
  auto le = transform(lexpr);
  auto re = CAST(rexpr, NoneExpr) ? clone(rexpr) : transform(rexpr);
  if (le->getType()->getUnbound() || (op != "is" && re->getType()->getUnbound())) {
    auto e = N<BinaryExpr>(move(le), op, move(re));
    e->setType(ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel));
    return e;
  } else if (op == "&&" || op == "||") {
    auto e = N<BinaryExpr>(move(le), op, move(re));
    e->setType(ctx->findInternal(".bool"));
    return e;
  } else if (op == "is") {
    if (CAST(rexpr, NoneExpr)) {
      if (le->getType()->getClass()->name != ".Optional")
        return transform(N<BoolExpr>(false));
      else
        return transform(N<CallExpr>(
            N<DotExpr>(N<CallExpr>(N<DotExpr>(move(le), "__bool__")), "__invert__")));
    }
    ExprPtr e;
    if (!le->getType()->canRealize() || !re->getType()->canRealize()) {
      e = N<BinaryExpr>(move(le), op, move(re));
    } else {
      auto lc = realizeType(le->getType()->getClass());
      auto rc = realizeType(re->getType()->getClass());
      if (!lc || !rc)
        error("both sides of 'is' expression must be of same reference type");
      e = transform(N<BinaryExpr>(N<CallExpr>(N<DotExpr>(move(le), "__raw__")),
                                  "==", N<CallExpr>(N<DotExpr>(move(re), "__raw__"))));
    }
    e->setType(ctx->findInternal(".bool"));
    return e;
  } else {
    auto mi = magics.find(op);
    if (mi == magics.end())
      error("invalid binary operator '{}'", op);
    auto magic = mi->second;
    auto lc = le->getType()->getClass(), rc = re->getType()->getClass();
    assert(lc && rc);
    auto plc = ctx->instantiateGeneric(getSrcInfo(), ctx->findInternal(".Ptr"), {lc});
    FuncTypePtr f;
    if (isAtomic &&
        (f = findBestCall(lc, format("__atomic_{}__", magic), {{"", plc}, {"", rc}}))) {
      le = N<PtrExpr>(move(le));
      if (noReturn)
        *noReturn = true;
    } else if (inPlace &&
               (f = findBestCall(lc, format("__i{}__", magic), {{"", lc}, {"", rc}}))) {
      if (noReturn)
        *noReturn = true;
    } else if ((f = findBestCall(lc, format("__{}__", magic), {{"", lc}, {"", rc}}))) {
      ;
    } else if ((f = findBestCall(rc, format("__r{}__", magic), {{"", rc}, {"", lc}}))) {
      ;
    } else {
      error("cannot find magic '{}' for {}", magic, lc->toString());
    }
    return transform(N<CallExpr>(N<IdExpr>(f->name), move(le), move(re)));
  }
}

void TypecheckVisitor::visit(const PipeExpr *expr) {
  auto extractType = [&](TypePtr t) {
    auto c = t->getClass();
    if (c && c->name == ".Generator")
      return c->explicits[0].type;
    else
      return t;
  };

  vector<PipeExpr::Pipe> items;
  items.push_back({expr->items[0].op, transform(expr->items[0].expr)});
  vector<types::TypePtr> types;
  TypePtr inType = items.back().expr->getType();
  types.push_back(inType);
  inType = extractType(inType);
  int inTypePos = 0;
  for (int i = 1; i < expr->items.size(); i++) {
    auto l = expr->items[i].clone();

  reset:
    if (auto ce = CAST(l.expr, CallExpr)) {
      // TODO: what if this is a StmtExpr [e.g. a constructor]?
      int inTypePos = -1;
      for (int ia = 0; ia < ce->args.size(); ia++)
        if (auto ee = CAST(ce->args[ia].value, EllipsisExpr)) {
          if (inTypePos == -1)
            ee->isPipeArg = true, inTypePos = ia;
          else
            error(ce->args[ia].value, "unexpected partial argument");
        }
      if (inTypePos == -1) {
        ce->args.insert(ce->args.begin(), {"", N<EllipsisExpr>(true)});
        inTypePos = 0;
      }

      // forceUnify(ce->args[inTypePos].value, inType);
      ExprPtr st = nullptr;
      auto n = parseCall(ce, inType, &st);
      if (st) {
        l = {"|>", move(st)};
        i--;
        goto reset;
      }
      items.push_back({l.op, move(n)});
    } else {
      auto c = N<CallExpr>(clone(l.expr), N<EllipsisExpr>(true));
      // forceUnify(c->args[0].value, inType);

      ExprPtr st = nullptr;
      auto n = parseCall(c.get(), inType, &st);
      if (st) {
        l = {"|>", move(st)};
        i--;
        goto reset;
      }
      items.push_back({l.op, move(n)});
      inTypePos = 0;
    }
    inType = items.back().expr->getType();
    types.push_back(inType);

    if (i < expr->items.size() - 1)
      inType = extractType(inType);
  }
  resultExpr = N<PipeExpr>(move(items));
  CAST(resultExpr, PipeExpr)->inTypes = types;
  resultExpr->setType(forceUnify(expr, inType));
}

void TypecheckVisitor::visit(const StaticExpr *expr) {
  // when visited "normally" just treat it as normal expression
  resultExpr = transform(expr->expr);
}

void TypecheckVisitor::visit(const InstantiateExpr *expr) {
  ExprPtr e = transform(expr->typeExpr, true);
  auto g = ctx->instantiate(e->getSrcInfo(), e->getType());
  for (int i = 0; i < expr->typeParams.size(); i++) {
    TypePtr t = nullptr;
    if (auto s = CAST(expr->typeParams[i], StaticExpr)) {
      map<string, Generic> m;
      for (auto g : s->captures) {
        auto val = ctx->find(g);
        assert(val && val->isStatic());
        auto t = val->getType()->follow();
        m[g] = {g, t,
                t->getLink()
                    ? t->getLink()->id
                    : t->getStatic()->explicits.size() ? t->getStatic()->explicits[0].id
                                                       : 0};
      }
      auto sv = StaticVisitor(m);
      sv.transform(s->expr);
      if (auto ie = CAST(s->expr, IdExpr)) { /// special case: generic static expr
        assert(m.size() == 1);
        t = ctx->instantiate(getSrcInfo(), m.begin()->second.type);
      } else {
        vector<Generic> v;
        for (auto &i : m)
          v.push_back(i.second);
        t = make_shared<StaticType>(v, clone(s->expr));
      }
    } else {
      if (!expr->typeParams[i]->isType())
        error(expr->typeParams[i], "not a type");
      t = ctx->instantiate(getSrcInfo(), transformType(expr->typeParams[i])->getType());
    }
    /// Note: at this point, only single-variable static var expression (e.g.
    /// N) is allowed, so unify will work as expected.
    if (g->getFunc()) {
      if (i >= g->getFunc()->explicits.size())
        error("expected {} generics", g->getFunc()->explicits.size());
      forceUnify(g->getFunc()->explicits[i].type, t);
    } else {
      if (i >= g->getClass()->explicits.size())
        error("expected {} generics", g->getClass()->explicits.size());
      forceUnify(g->getClass()->explicits[i].type, t);
    }
  }
  bool isType = e->isType();
  auto t = forceUnify(expr, g);
  auto newName = patchIfRealizable(t, isType);
  auto i = CAST(e, IdExpr);
  if (!newName.empty() && i && newName != i->value) {
    auto comp = split(newName, ':');
    if (startswith(comp.back(), i->value))
      i->value = newName;
  }
  resultExpr = move(e); // will get replaced by identifier later on
  if (isType)
    resultExpr->markType();
  resultExpr->setType(t);
}

void TypecheckVisitor::visit(const SliceExpr *expr) {
  ExprPtr none = N<CallExpr>(N<DotExpr>(N<IdExpr>(".Optional"), "__new__"));
  resultExpr = transform(N<CallExpr>(N<IdExpr>(".Slice"),
                                     expr->start ? clone(expr->start) : transform(none),
                                     expr->stop ? clone(expr->stop) : transform(none),
                                     expr->step ? clone(expr->step) : transform(none)));
}

void TypecheckVisitor::visit(const IndexExpr *expr) {
  auto getTupleIndex = [&](ClassType *tuple, const auto &expr,
                           const auto &index) -> ExprPtr {
    if (!tuple->isRecord())
      return nullptr;
    if (tuple->name == ".Ptr" || tuple->name == ".Array" || tuple->name == ".Optional")
      return nullptr;
    if (!startswith(tuple->name, ".Tuple.")) { // avoid if there is a __getitem__ here
      // auto m = ctx->findMethod(tuple->name, "__getitem__");
      // if (m && m->size() > 1)
      return nullptr;
      // TODO : be smarter! there might be a compatible getitem?
    }
    auto mm = ctx->cache->classes.find(tuple->name);
    assert(mm != ctx->cache->classes.end());
    auto getInt = [](seq_int_t *o, const ExprPtr &e) {
      if (!e)
        return true;
      if (auto i = CAST(e, IntExpr)) {
        *o = i->intValue;
        return true;
      }
      return false;
    };
    seq_int_t s = 0, e = tuple->args.size(), st = 1;
    if (auto ex = CAST(index, IntExpr)) {
      int i = translateIndex(ex->intValue, e);
      if (i < 0 || i >= e)
        error("tuple index out of range (expected 0..{}, got {})", e, i);
      return transform(N<DotExpr>(clone(expr), mm->second.fields[i].name));
    } else if (auto sx = CAST(index, StaticExpr)) {
      map<string, types::Generic> m;
      auto sv = StaticVisitor(m);
      auto r = sv.transform(sx->expr);
      assert(r.first);
      int i = translateIndex(r.second, e);
      if (i < 0 || i >= e)
        error("tuple index out of range (expected 0..{}, got {})", e, i);
      return transform(N<DotExpr>(clone(expr), mm->second.fields[i].name));
    } else if (auto i = CAST(index, SliceExpr)) {
      if (!getInt(&s, i->start) || !getInt(&e, i->stop) || !getInt(&st, i->step))
        return nullptr;
      if (i->step && !i->start)
        s = st > 0 ? 0 : tuple->args.size();
      if (i->step && !i->stop)
        e = st > 0 ? tuple->args.size() : 0;
      sliceAdjustIndices(tuple->args.size(), &s, &e, st);
      vector<ExprPtr> te;
      for (auto i = s; (st >= 0) ? (i < e) : (i >= e); i += st) {
        if (i < 0 || i >= tuple->args.size())
          error("tuple index out of range (expected 0..{}, got {})", tuple->args.size(),
                i);
        te.push_back(N<DotExpr>(clone(expr), mm->second.fields[i].name));
      }
      return transform(N<CallExpr>(
          N<DotExpr>(N<IdExpr>(format(".Tuple.{}", te.size())), "__new__"), move(te)));
    }
    return nullptr;
  };

  ExprPtr e = transform(expr->expr, true);
  auto t = e->getType();
  if (t->getFunc()) {
    vector<ExprPtr> it;
    if (auto t = CAST(expr->index, TupleExpr))
      for (auto &i : t->items)
        it.push_back(clone(i));
    else
      it.push_back(clone(expr->index));
    resultExpr = transform(N<InstantiateExpr>(move(e), move(it)));
  } else if (auto c = t->getClass()) {
    resultExpr = getTupleIndex(c.get(), expr->expr, expr->index);
    if (!resultExpr)
      resultExpr = transform(N<CallExpr>(N<DotExpr>(expr->expr->clone(), "__getitem__"),
                                         expr->index->clone()));
  } else {
    resultExpr = N<IndexExpr>(move(e), transform(expr->index));
    resultExpr->setType(
        forceUnify(expr, ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel)));
  }
}

void TypecheckVisitor::visit(const StackAllocExpr *expr) {
  auto te = transformType(expr->typeExpr);
  auto e = transform(expr->expr);

  auto t = te->getType();
  resultExpr = N<StackAllocExpr>(move(te), move(e));
  t = ctx->instantiateGeneric(expr->getSrcInfo(), ctx->findInternal(".Array"), {t});
  patchIfRealizable(t, true);
  resultExpr->setType(forceUnify(expr, t));
}

ExprPtr TypecheckVisitor::visitDot(const DotExpr *expr, vector<CallExpr::Arg> *args) {
  auto isMethod = [&](FuncTypePtr f) {
    auto ast = ctx->cache->functions[f->name].ast.get();
    return in(ast->attributes, ATTR_NOT_STATIC);
  };
  auto deactivateUnbounds = [&](Type *t) {
    auto ub = t->getUnbounds();
    for (auto &u : ub)
      ctx->activeUnbounds.erase(u);
  };

  auto lhs = transform(expr->expr, true);
  TypePtr typ = nullptr;
  if (lhs->getType()->getUnbound()) {
    typ = ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel);
  } else if (auto c = lhs->getType()->getClass()) {
    auto m = ctx->findMethod(c->name, expr->member);
    if (!m.empty()) {
      if (args) {
        vector<pair<string, TypePtr>> targs;
        if (!lhs->isType())
          targs.emplace_back(make_pair("", c));
        for (auto &a : *args)
          targs.emplace_back(make_pair(a.name, a.value->getType()));
        if (auto m = findBestCall(c, expr->member, targs, true)) {
          if (!lhs->isType())
            args->insert(args->begin(), {"", clone(lhs)});
          auto e = N<IdExpr>(m->name);
          e->setType(ctx->instantiate(getSrcInfo(), m, c.get()));
          if (lhs->isType() && !isMethod(m))
            deactivateUnbounds(c.get());
          return e;
        } else {
          vector<string> nice;
          for (auto &t : targs)
            nice.emplace_back(format("{} = {}", t.first, t.second->toString()));
          error("cannot find method '{}' in {} with arguments {}", expr->member,
                c->toString(), join(nice, ", "));
        }
      }

      FuncTypePtr bestCall = nullptr;
      if (m.size() > 1) {
        // need to check is this a callable that we can use to instantiate the type
        if (expr->getType() && expr->getType()->getClass()) {
          auto dc = expr->getType()->getClass();
          if (startswith(dc->name, ".Function.")) {
            vector<pair<string, TypePtr>> targs; // we can, well, unify this
            if (!lhs->isType())
              targs.emplace_back(make_pair("", c));
            for (auto i = 1; i < dc->explicits.size(); i++)
              targs.emplace_back(make_pair("", dc->explicits[i].type));
            if (auto mc = findBestCall(c, expr->member, targs, true))
              bestCall = mc;
            else {
              vector<string> nice;
              for (auto &t : targs)
                nice.emplace_back(format("{} = {}", t.first, t.second->toString()));
              error("cannot find method '{}' in {} with arguments {}", expr->member,
                    c->toString(), join(nice, ", "));
            }
          }
        }
      } else {
        bestCall = m[0];
      }
      if (!bestCall) {
        // TODO: fix this and have better method for handling these cases
        bestCall = m[0];
      }
      if (lhs->isType()) {
        auto name = bestCall->name;
        auto val = ctx->find(name);
        assert(val);
        auto t = ctx->instantiate(getSrcInfo(), bestCall, c.get());
        auto e = N<IdExpr>(name);
        e->setType(t);
        auto newName = patchIfRealizable(t, val->isType());
        if (!newName.empty())
          e->value = newName;
        if (!isMethod(bestCall))
          deactivateUnbounds(c.get());
        return e;
      } else { // cast y.foo to CLS.foo(y, ...)
        auto f = bestCall;
        vector<ExprPtr> args;
        args.push_back(move(lhs));
        for (int i = 0; i < std::max(1, (int)f->args.size() - 2); i++)
          args.push_back(N<EllipsisExpr>());
        auto ast = ctx->cache->functions[f->name].ast.get();
        if (in(ast->attributes, "property"))
          args.pop_back();
        return transform(N<CallExpr>(N<IdExpr>(bestCall->name), move(args)));
      }
    } else if (auto mm = ctx->findMember(c->name, expr->member)) {
      typ = ctx->instantiate(getSrcInfo(), mm, c.get());
    } else if (c->name == ".Optional") {
      auto d =
          N<DotExpr>(transform(N<CallExpr>(N<IdExpr>(".unwrap"), clone(expr->expr))),
                     expr->member);
      return visitDot(d.get(), args);
    } else if (c->name == ".pyobj") {
      return transform(N<CallExpr>(N<DotExpr>(clone(expr->expr), "_getattr"),
                                   N<StringExpr>(expr->member)));
    } else {
      error("cannot find '{}' in {}", expr->member, lhs->getType()->toString());
    }
  } else {
    error("cannot find '{}' in {}", expr->member, lhs->getType()->toString());
  }
  auto t = N<DotExpr>(move(lhs), expr->member);
  t->setType(typ);
  return t;
}

void TypecheckVisitor::visit(const CallExpr *expr) { resultExpr = parseCall(expr); }

ExprPtr TypecheckVisitor::parseCall(const CallExpr *expr, types::TypePtr inType,
                                    ExprPtr *extraStage) {
  vector<CallExpr::Arg> args;
  for (auto &i : expr->args) {
    args.push_back({i.name, transform(i.value)});
    if (auto e = CAST(i.value, EllipsisExpr)) {
      if (inType && e->isPipeArg &&
          !inType->getUnbound()) // if unbound, might be a generator and unpack later
        forceUnify(inType, args.back().value->getType());
      else
        forceUnify(i.value, args.back().value->getType());
    }
  }

  ExprPtr callee = nullptr;
  Expr *lhs = const_cast<CallExpr *>(expr)->expr.get();
  if (auto i = CAST(expr->expr, IndexExpr))
    lhs = i->expr.get();
  else if (auto i = CAST(expr->expr, InstantiateExpr))
    lhs = i->typeExpr.get();
  if (auto i = dynamic_cast<DotExpr *>(lhs)) {
    callee = visitDot(i, &args);
    if (auto i = CAST(expr->expr, IndexExpr))
      callee = transform(N<IndexExpr>(move(callee), clone(i->index)));
    else if (auto i = CAST(expr->expr, InstantiateExpr))
      callee = transform(N<InstantiateExpr>(move(callee), clone(i->typeParams)));
  } else {
    callee = transform(expr->expr, true);
  }
  forceUnify(expr->expr.get(), callee->getType());

  auto calleeType = callee->getType();
  auto calleeClass = callee->getType()->getClass();
  if (!calleeClass) { // Unbound caller, will be handled later
    callee = N<CallExpr>(move(callee), move(args));
    callee->setType(
        forceUnify(expr, ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel)));
    return callee;
  } else if (callee->isType() && calleeClass->isRecord()) {
    return transform(N<CallExpr>(N<DotExpr>(move(callee), "__new__"), move(args)));
  } else if (callee->isType()) {
    /// WARN: passing callee & args that have already been transformed
    ExprPtr var = N<IdExpr>(ctx->cache->getTemporaryVar("v"));
    vector<StmtPtr> stmts;
    stmts.push_back(
        N<AssignStmt>(clone(var), N<CallExpr>(N<DotExpr>(move(callee), "__new__"))));
    stmts.push_back(
        N<ExprStmt>(N<CallExpr>(N<DotExpr>(clone(var), "__init__"), move(args))));
    return transform(N<StmtExpr>(move(stmts), clone(var)));
  } else if (!calleeClass->getCallable()) {
    if (calleeClass->name == ".pyobj") {
      if (args.size() != 1 ||
          !(args[0].value->getType()->getClass() &&
            startswith(args[0].value->getType()->getClass()->name, ".Tuple."))) {
        vector<ExprPtr> e;
        for (auto &a : args) {
          if (a.name != "")
            error("named python calls are not yet supported");
          e.push_back(move(a.value));
        }
        auto ne = transform(N<CallExpr>(
            N<DotExpr>(N<IdExpr>(format(".Tuple.{}", args.size())), "__new__"),
            move(e)));
        args.clear();
        args.push_back({"", move(ne)});
      }
    }
    return transform(N<CallExpr>(N<DotExpr>(move(callee), "__call__"), move(args)));
  }

  FunctionStmt *ast = nullptr;
  if (auto ff = calleeType->getFunc()) {
    ast = ctx->cache->functions[ff->name].ast.get();
  }

  // Handle named and default arguments
  vector<CallExpr::Arg> reorderedArgs;
  vector<int> argIndex;
  string knownTypes;
  if (startswith(calleeClass->name, ".Partial.")) {
    knownTypes = calleeClass->name.substr(9);
    calleeType = calleeClass->args[0];
    calleeClass = calleeClass->args[0]->getClass();
    assert(calleeClass);
  }
  for (int i = 0; i < int(calleeClass->args.size()) - 1; i++)
    if (knownTypes.empty() || knownTypes[i] == '0')
      argIndex.push_back(i);

  vector<int> pending;
  bool isPartial = false;
  bool namesStarted = false;
  unordered_map<string, ExprPtr> namedArgs;
  for (int i = 0; i < args.size(); i++) {
    if (args[i].name == "" && namesStarted)
      error("unnamed argument after a named argument");
    namesStarted |= args[i].name != "";
    if (args[i].name == "")
      reorderedArgs.push_back({"", move(args[i].value)});
    else if (namedArgs.find(args[i].name) == namedArgs.end())
      namedArgs[args[i].name] = move(args[i].value);
    else
      error("named argument {} repeated multiple times", args[i].name);
  }

  if (namedArgs.size() == 0 && reorderedArgs.size() == argIndex.size() + 1 &&
      CAST(reorderedArgs.back().value, EllipsisExpr)) {
    isPartial = true;
    forceUnify(reorderedArgs.back().value, ctx->findInternal(".void"));
    reorderedArgs.pop_back();
  } else if (reorderedArgs.size() + namedArgs.size() > argIndex.size()) {
    error("too many arguments for {} (expected {}, got {})", calleeType->toString(),
          argIndex.size(), reorderedArgs.size() + namedArgs.size());
  }

  if (ast) {
    ctx->addBlock();
    addFunctionGenerics(calleeType->getFunc());
  } else if (!ast && namedArgs.size()) {
    error("unexpected name '{}' (function pointers have argument names elided)",
          namedArgs.begin()->first);
  }

  for (int i = 0, ra = reorderedArgs.size(); i < argIndex.size(); i++) {
    if (i >= ra) {
      assert(ast);
      auto it = namedArgs.find(ast->args[argIndex[i]].name);
      if (it != namedArgs.end()) {
        reorderedArgs.push_back({"", move(it->second)});
        namedArgs.erase(it);
      } else if (ast->args[argIndex[i]].deflt) {
        reorderedArgs.push_back({"", transform(ast->args[argIndex[i]].deflt)});
      } else {
        error("argument '{}' missing", ast->args[argIndex[i]].name);
      }
    }
    if (auto ee = CAST(reorderedArgs[i].value, EllipsisExpr))
      if (!ee->isPipeArg)
        pending.push_back(argIndex[i]);
  }
  for (auto &i : namedArgs)
    error(i.second, "unknown argument {}", i.first);
  if (isPartial || pending.size())
    pending.push_back(args.size());

  // Unification stage
  bool unificationsDone = true;
  for (int ri = 0; ri < reorderedArgs.size(); ri++) {
    auto sigType = calleeClass->args[argIndex[ri] + 1]->getClass();
    auto argType = reorderedArgs[ri].value->getType()->getClass();

    if (sigType && (sigType->isTrait || sigType->name == ".Optional")) {
      // Case 0: type not yet known
      if (!argType) /* && !(reorderedArgs[ri].value->getType()->getUnbound() &&
                        reorderedArgs[ri]
                            .value->getType()
                            ->getUnbound()
                            ->treatAsClass)) { // do not unify if not yet known */
      {
        unificationsDone = false;
      }
      // Case 1: generator wrapping
      else if (sigType->name == ".Generator" && argType &&
               argType->name != sigType->name && !extraStage) {
        // do not do this in pipelines
        reorderedArgs[ri].value = transform(
            N<CallExpr>(N<DotExpr>(move(reorderedArgs[ri].value), "__iter__")));
        forceUnify(reorderedArgs[ri].value,
                   calleeClass->args[argIndex[ri] + 1]); // sigType; needs Type* in
                                                         // unify for nicer interface
      }
      // Case 2: optional wrapping
      else if (sigType->name == ".Optional" && argType &&
               argType->name != sigType->name) {
        if (extraStage && CAST(reorderedArgs[ri].value, EllipsisExpr)) {
          *extraStage = N<DotExpr>(N<IdExpr>(".Optional"), "__new__");
          return expr->clone();
        } else {
          reorderedArgs[ri].value = transform(
              N<CallExpr>(N<IdExpr>(".Optional"), move(reorderedArgs[ri].value)));
          forceUnify(reorderedArgs[ri].value, calleeClass->args[argIndex[ri] + 1]);
        }
      }
      // Case 3: Callables
      // TODO: this is only allowed with Seq function calls;
      // this won't be done with Function[] pointers or similar
      // as it is not trivial to cast Partial to Function[]
      else if (ast && startswith(sigType->name, ".Function.") && argType &&
               !startswith(argType->name, ".Function.")) {
        if (!startswith(argType->name, ".Partial.")) {
          reorderedArgs[ri].value =
              transform(N<DotExpr>(move(reorderedArgs[ri].value), "__call__"));
          argType = reorderedArgs[ri].value->getType()->getClass();
        }
        if (argType && startswith(argType->name, ".Partial.")) {
          forceUnify(argType->explicits[0].type, sigType->explicits[0].type);
          if (argType->explicits.size() != sigType->explicits.size() + 1)
            error("incompatible partial type");
          for (int j = 1; j < sigType->explicits.size(); j++)
            forceUnify(argType->explicits[j + 1].type, sigType->explicits[j].type);

          callee->getType()->getFunc()->args[ri + 1] =
              reorderedArgs[ri].value->getType(); // argType
        } else {
          forceUnify(reorderedArgs[ri].value, calleeClass->args[argIndex[ri] + 1]);
        }
      }
      // Otherwise, just unify as-is
      else {
        forceUnify(reorderedArgs[ri].value, calleeClass->args[argIndex[ri] + 1]);
      }
    } else if (sigType && argType && argType->name == ".Optional") { // unwrap optional
      if (extraStage && CAST(reorderedArgs[ri].value, EllipsisExpr)) {
        *extraStage = N<IdExpr>(".unwrap");
        return expr->clone();
      } else {
        reorderedArgs[ri].value =
            transform(N<CallExpr>(N<IdExpr>(".unwrap"), move(reorderedArgs[ri].value)));
        forceUnify(reorderedArgs[ri].value, calleeClass->args[argIndex[ri] + 1]);
      }
    } else {
      forceUnify(reorderedArgs[ri].value, calleeClass->args[argIndex[ri] + 1]);
    }
  }
  if (ast)
    ctx->popBlock();

  // Realize functions that are passed as arguments
  auto fix = [&](ExprPtr &callee, const string &newName) {
    auto i = CAST(callee, IdExpr);
    if (!i || newName == i->value)
      return;
    auto comp = split(newName, ':');
    if (startswith(comp.back(), i->value))
      i->value = newName;
  };
  for (auto &ra : reorderedArgs)
    if (ra.value->getType()->getFunc() && ra.value->getType()->canRealize()) {
      auto r = realizeFunc(ra.value->getType());
      fix(ra.value, r->realizeString());
    }
  if (auto f = calleeType->getFunc()) {
    // Handle default generics (callee.g. foo[S, T=int])
    for (int i = 0; i < f->explicits.size(); i++)
      if (auto l = f->explicits[i].type->getLink()) {
        if (unificationsDone && l && l->kind == LinkType::Unbound &&
            ast->generics[i].deflt) {
          auto t = transformType(ast->generics[i].deflt);
          forceUnify(l, t->getType());
        }
      }
    if (f->canRealize()) {
      auto r = realizeFunc(f);
      if (knownTypes.empty())
        fix(callee, r->realizeString());
    }
  }

  // Emit final call
  if (pending.size()) { // (still) partial?
    pending.pop_back();
    string known(calleeClass->args.size() - 1, '1');
    for (auto p : pending)
      known[p] = '0';
    auto pt = generatePartialStub(known, knownTypes);
    vector<ExprPtr> a;
    a.push_back(move(callee));
    for (auto &r : reorderedArgs)
      if (!CAST(r.value, EllipsisExpr))
        a.push_back(move(r.value));
    callee = transform(N<CallExpr>(N<IdExpr>(pt), move(a)));
    forceUnify(expr, callee->getType());
    return callee;
  } else if (knownTypes.empty()) { // normal function
    callee = N<CallExpr>(move(callee), move(reorderedArgs));
    callee->setType(forceUnify(expr, calleeClass->args[0]));
    return callee;
  } else { // partial that is fulfilled
    callee = transform(
        N<CallExpr>(N<DotExpr>(move(callee), "__call__"), move(reorderedArgs)));
    forceUnify(expr, callee->getType());
    return callee;
  }
} // namespace tmp

void TypecheckVisitor::visit(const DotExpr *expr) {
  resultExpr = visitDot(expr);
  forceUnify(expr, resultExpr->getType());
}

void TypecheckVisitor::visit(const EllipsisExpr *expr) {
  resultExpr = N<EllipsisExpr>(expr->isPipeArg);
  resultExpr->setType(ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel));
}

void TypecheckVisitor::visit(const TypeOfExpr *expr) {
  auto e = transform(expr->expr);
  auto t = forceUnify(expr, e->getType());

  auto newName = patchIfRealizable(t, true);
  if (!newName.empty())
    resultExpr = N<IdExpr>(newName);
  else
    resultExpr = N<TypeOfExpr>(move(e));
  resultExpr->markType();
  resultExpr->setType(t);
}

void TypecheckVisitor::visit(const PtrExpr *expr) {
  auto param = transform(expr->expr);
  auto t = param->getType();
  resultExpr = N<PtrExpr>(move(param));
  resultExpr->setType(
      forceUnify(expr, ctx->instantiateGeneric(expr->getSrcInfo(),
                                               ctx->findInternal(".Ptr"), {t})));
}

void TypecheckVisitor::visit(const YieldExpr *expr) {
  resultExpr = N<YieldExpr>();
  if (ctx->bases.size() <= 1)
    error("(yield) cannot be used outside of functions");
  auto t =
      ctx->instantiateGeneric(getSrcInfo(), ctx->findInternal(".Generator"),
                              {ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel)});
  auto &base = ctx->bases.back();
  if (base.returnType)
    t = forceUnify(base.returnType, t);
  else
    base.returnType = t;
  auto c = t->follow()->getClass();
  assert(c);
  resultExpr->setType(forceUnify(expr, c->explicits[0].type));
}

void TypecheckVisitor::visit(const StmtExpr *expr) {
  vector<StmtPtr> stmts;
  for (auto &s : expr->stmts)
    stmts.push_back(transform(s));
  auto e = transform(expr->expr);
  auto t = forceUnify(expr, e->getType());
  resultExpr = N<StmtExpr>(move(stmts), move(e));
  resultExpr->setType(t);
}

void TypecheckVisitor::visit(const SuiteStmt *stmt) {
  vector<StmtPtr> r;
  if (stmt->ownBlock)
    ctx->addBlock();
  for (auto &s : stmt->stmts)
    if (auto t = transform(s))
      r.push_back(move(t));
  if (stmt->ownBlock)
    ctx->popBlock();
  resultStmt = N<SuiteStmt>(move(r), stmt->ownBlock);
}

void TypecheckVisitor::visit(const ExprStmt *stmt) {
  resultStmt = N<ExprStmt>(transform(stmt->expr));
}

void TypecheckVisitor::visit(const AssignStmt *stmt) {
  auto l = stmt->lhs->getId();
  // LOG("{}", stmt->toString());
  seqassert(l, "invalid AssignStmt {}", stmt->toString());

  auto rhs = transform(stmt->rhs);
  auto typExpr = transformType(stmt->type);
  types::TypePtr t;
  TypecheckItem::Kind k;
  if (!rhs) { // declarations
    t = typExpr ? typExpr->getType()
                : ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel);
    ctx->add(k = TypecheckItem::Var, l->value, t);
  } else {
    if (typExpr && typExpr->getType()->getClass()) {
      auto typ = ctx->instantiate(getSrcInfo(), typExpr->getType());

      auto lc = typ->getClass();
      auto rc = rhs->getType()->getClass();
      if (lc && lc->name == ".Optional" && rc && rc->name != lc->name)
        rhs = transform(N<CallExpr>(N<IdExpr>(".Optional"), move(rhs)));

      forceUnify(typ, rhs->getType());
    }
    k = rhs->isType()
            ? TypecheckItem::Type
            : (rhs->getType()->getFunc() ? TypecheckItem::Func : TypecheckItem::Var);
    ctx->add(k, l->value, t = rhs->getType());
  }
  if (l->value[0] == '.')
    ctx->bases.back().visitedAsts[l->value] = {k, t};
  auto lhs = clone(stmt->lhs);
  lhs->setType(forceUnify(lhs, t));
  resultStmt = N<AssignStmt>(move(lhs), move(rhs), move(typExpr));
}

void TypecheckVisitor::visit(const UpdateStmt *stmt) {
  auto l = transform(stmt->lhs);
  auto lc = l->getType()->getClass();
  ExprPtr r = nullptr;

  auto b = CAST(stmt->rhs, BinaryExpr);
  if (b && b->inPlace) {
    bool noReturn = false;
    auto e =
        transformBinary(b->lexpr, b->rexpr, b->op, true, stmt->isAtomic, &noReturn);
    e->setType(forceUnify(stmt->rhs, e->getType()));
    if (CAST(e, BinaryExpr)) { // decide later...
      l->setType(forceUnify(e.get(), l->getType()));
      resultStmt = N<UpdateStmt>(move(l), move(e), stmt->isAtomic);
      return;
    } else if (noReturn) { // Remove assignment, just use update stuff
      resultStmt = N<ExprStmt>(move(e));
      return;
    } else {
      r = move(e);
    }
  }
  // detect min/max: a = min(a, ...) (not vice-versa...)

  bool atomic = stmt->isAtomic;
  const CallExpr *c;
  if (atomic && l->getId() && (c = stmt->rhs->getCall()) &&
      (c->expr->isId(".min") || c->expr->isId(".max")) && c->args.size() == 2 &&
      c->args[0].value->isId(string(l->getId()->value))) {
    auto pt = ctx->instantiateGeneric(getSrcInfo(), ctx->findInternal(".Ptr"), {lc});
    auto rsh = transform(c->args[1].value);
    auto rc = rsh->getType()->getClass();
    if (auto m =
            findBestCall(lc, format("__atomic_{}__", chop(c->expr->getId()->value)),
                         {{"", pt}, {"", rc}})) {
      resultStmt = transform(
          N<ExprStmt>(N<CallExpr>(N<IdExpr>(m->name), N<PtrExpr>(move(l)), move(rsh))));
      return;
    }
  }

  if (!r)
    r = transform(stmt->rhs);
  auto rc = r->getType()->getClass();
  if (atomic && lc && rc) { // maybe an atomic = ?
    auto pt = ctx->instantiateGeneric(getSrcInfo(), ctx->findInternal(".Ptr"), {lc});
    if (auto m = findBestCall(lc, "__atomic_xchg__", {{"", pt}, {"", rc}})) {
      resultStmt = transform(
          N<ExprStmt>(N<CallExpr>(N<IdExpr>(m->name), N<PtrExpr>(move(l)), move(r))));
      return;
    } else {
      atomic = false;
    }
  } else if (lc && lc->name == ".Optional" && rc && rc->name != lc->name) {
    r = transform(N<CallExpr>(N<IdExpr>(".Optional"), move(r)));
  }
  l->setType(forceUnify(r.get(), l->getType()));
  resultStmt = N<UpdateStmt>(move(l), move(r), atomic);
} // namespace ast

void TypecheckVisitor::visit(const AssignMemberStmt *stmt) {
  auto lh = transform(stmt->lhs);
  auto rh = transform(stmt->rhs);
  auto lc = lh->getType()->getClass();
  auto rc = rh->getType()->getClass();

  if (lc) {
    auto mm = ctx->findMember(lc->name, stmt->member);
    if (!mm && lc->name == ".Optional") {
      resultStmt = transform(N<AssignMemberStmt>(
          N<CallExpr>(N<IdExpr>(".unwrap"), clone(stmt->lhs)), stmt->member, move(rh)));
      return;
    }
    if (!mm)
      error("cannot find '{}'", stmt->member);

    if (lc && lc->isRecord())
      error("records are read-only ^ {} , {}", lc->toString(), lh->toString());

    auto t = ctx->instantiate(getSrcInfo(), mm, lc.get());
    lc = t->getClass();
    if (lc && lc->name == ".Optional" && rc && rc->name != lc->name)
      rh = transform(N<CallExpr>(N<IdExpr>(".Optional"), move(rh)));
    forceUnify(t, rh->getType());
  }

  resultStmt = N<AssignMemberStmt>(move(lh), stmt->member, move(rh));
}

void TypecheckVisitor::visit(const ReturnStmt *stmt) {
  auto n = ctx->bases.back().name;
  if (stmt->expr) {
    auto e = transform(stmt->expr);
    auto &base = ctx->bases.back();

    if (base.returnType) {
      auto l = base.returnType->getClass();
      auto r = e->getType()->getClass();
      if (l && r && r->name != l->name) {
        if (l->name == ".Optional") {
          e = transform(N<CallExpr>(N<IdExpr>(".Optional"), move(e)));
        }
        // For now this only works if we already know that returnType is optional
      }
      forceUnify(e->getType(), base.returnType);
    } else {
      base.returnType = e->getType();
    }

    // HACK for return void in Partial.__call__
    if (startswith(base.name, ".Partial.") && endswith(base.name, ".__call__")) {
      auto c = e->getType()->getClass();
      if (c && c->name == ".void") {
        resultStmt = N<ExprStmt>(move(e));
        return;
      }
    }
    resultStmt = N<ReturnStmt>(move(e));
  } else {
    resultStmt = N<ReturnStmt>(nullptr);
  }
}

void TypecheckVisitor::visit(const YieldStmt *stmt) {
  types::TypePtr t = nullptr;
  if (stmt->expr) {
    auto e = transform(stmt->expr);
    t = ctx->instantiateGeneric(e->getSrcInfo(), ctx->findInternal(".Generator"),
                                {e->getType()});
    resultStmt = N<YieldStmt>(move(e));
  } else {
    t = ctx->instantiateGeneric(stmt->getSrcInfo(), ctx->findInternal(".Generator"),
                                {ctx->findInternal(".void")});
    resultStmt = N<YieldStmt>(nullptr);
  }
  auto &base = ctx->bases.back();
  if (base.returnType)
    forceUnify(t, base.returnType);
  else
    base.returnType = t;
}

void TypecheckVisitor::visit(const DelStmt *stmt) {
  auto expr = CAST(stmt->expr, IdExpr);
  ctx->remove(expr->value);
}

void TypecheckVisitor::visit(const WhileStmt *stmt) {
  resultStmt = N<WhileStmt>(transform(stmt->cond), transform(stmt->suite));
}

void TypecheckVisitor::visit(const ForStmt *stmt) {
  auto iter = transform(stmt->iter);
  TypePtr varType = ctx->addUnbound(stmt->var->getSrcInfo(), ctx->typecheckLevel);
  if (!iter->getType()->getUnbound()) {
    auto iterType = iter->getType()->getClass();
    if (!iterType || iterType->name != ".Generator")
      error(iter, "expected a generator");
    forceUnify(varType, iterType->explicits[0].type);
  }
  ctx->addBlock();
  auto i = CAST(stmt->var, IdExpr);
  assert(i);
  string varName = i->value;
  ctx->add(TypecheckItem::Var, varName, varType);
  resultStmt = N<ForStmt>(transform(stmt->var), move(iter), transform(stmt->suite));
  ctx->popBlock();
}

void TypecheckVisitor::visit(const IfStmt *stmt) {
  vector<IfStmt::If> ifs;
  for (auto &i : stmt->ifs)
    ifs.push_back({transform(i.cond), transform(i.suite)});
  resultStmt = N<IfStmt>(move(ifs));
}

void TypecheckVisitor::visit(const MatchStmt *stmt) {
  auto w = transform(stmt->what);
  auto matchType = w->getType();
  auto matchTypeClass = matchType->getClass();

  auto unifyType = [&](TypePtr t) {
    // auto tc = t->getClass();
    // if (tc && tc->name == ".seq" && matchTypeClass && matchTypeClass->name ==
    // ".Kmer")
    //   return;
    assert(t && matchType);
    types::Type::Unification us;
    us.isMatch = true;
    if (t->unify(matchType, us) < 0) {
      us.undo();
      error("cannot unify {} and {}", t->toString(), matchType->toString());
    }
  };

  vector<PatternPtr> patterns;
  vector<StmtPtr> cases;
  for (auto ci = 0; ci < stmt->cases.size(); ci++) {
    ctx->addBlock();
    if (auto p = CAST(stmt->patterns[ci], BoundPattern)) {
      auto boundPat = transform(p->pattern);
      ctx->add(TypecheckItem::Var, p->var, boundPat->getType());
      patterns.push_back(move(boundPat));
      unifyType(patterns.back()->getType());
      cases.push_back(transform(stmt->cases[ci]));
    } else {
      patterns.push_back(transform(stmt->patterns[ci]));
      unifyType(patterns.back()->getType());
      cases.push_back(transform(stmt->cases[ci]));
    }
    ctx->popBlock();
  }
  resultStmt = N<MatchStmt>(move(w), move(patterns), move(cases));
}

void TypecheckVisitor::visit(const TryStmt *stmt) {
  vector<TryStmt::Catch> catches;
  auto suite = transform(stmt->suite);
  for (auto &c : stmt->catches) {
    ctx->addBlock();
    auto exc = transformType(c.exc);
    if (c.var != "")
      ctx->add(TypecheckItem::Var, c.var, exc->getType());
    catches.push_back({c.var, move(exc), transform(c.suite)});
    ctx->popBlock();
  }
  resultStmt = N<TryStmt>(move(suite), move(catches), transform(stmt->finally));
}

void TypecheckVisitor::visit(const ThrowStmt *stmt) {
  resultStmt = N<ThrowStmt>(transform(stmt->expr));
}

void TypecheckVisitor::visit(const FunctionStmt *stmt) {
  resultStmt = N<FunctionStmt>(stmt->name, nullptr, vector<Param>(), vector<Param>(),
                               nullptr, map<string, string>(stmt->attributes));
  bool isClassMember = in(stmt->attributes, ATTR_PARENT_CLASS);

  if (auto t = ctx->findInVisited(stmt->name).second) {
    // seeing these for the second time, realize them (not in the preamble though)
    if (in(stmt->attributes, ATTR_BUILTIN) || in(stmt->attributes, ATTR_EXTERN_C)) {
      if (!t->canRealize())
        error("builtins and external functions must be realizable");
      realizeFunc(ctx->instantiate(getSrcInfo(), t)->getFunc());
    }
    return;
  }

  auto &attributes = const_cast<FunctionStmt *>(stmt)->attributes;

  ctx->addBlock();
  auto explicits = parseGenerics(stmt->generics, ctx->typecheckLevel); // level down
  vector<TypePtr> generics;
  // Iterate parent!!!
  if (isClassMember && in(attributes, ATTR_NOT_STATIC)) {
    auto c = ctx->cache->classes[attributes[ATTR_PARENT_CLASS]].ast.get();
    auto ct = ctx->find(attributes[ATTR_PARENT_CLASS])->type->getClass();
    assert(ct);
    for (int i = 0; i < c->generics.size(); i++) {
      auto l = ct->explicits[i].type->getLink();
      auto gt = make_shared<LinkType>(LinkType::Unbound, ct->explicits[i].id,
                                      ctx->typecheckLevel - 1, nullptr, l->isStatic);
      generics.push_back(gt);
      ctx->add(TypecheckItem::Type, c->generics[i].name, gt, true, l->isStatic);
    }
  }
  for (auto &i : stmt->generics)
    generics.push_back(ctx->find(i.name)->getType());

  ctx->typecheckLevel++;
  vector<TypePtr> args;
  if (stmt->ret) {
    args.push_back(transformType(stmt->ret)->getType());
  } else {
    args.push_back(ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel));
    generics.push_back(args.back());
  }
  for (auto &a : stmt->args) {
    args.push_back(a.type ? transformType(a.type)->getType()
                          : ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel));
    if (!a.type)
      generics.push_back(args.back());
    ctx->add(TypecheckItem::Var, a.name, args.back());
  }
  ctx->typecheckLevel--;
  for (auto &g : generics) { // Generalize generics
    assert(g && g->getLink() && g->getLink()->kind != types::LinkType::Link);
    if (g->getLink()->kind == LinkType::Unbound)
      g->getLink()->kind = LinkType::Generic;
  }
  ctx->popBlock();

  auto t = make_shared<FuncType>(
      stmt->name,
      ctx->findInternal(format(".Function.{}", stmt->args.size()))->getClass().get(),
      args, explicits);

  if (isClassMember && in(attributes, ATTR_NOT_STATIC)) {
    auto val = ctx->find(attributes[ATTR_PARENT_CLASS]);
    assert(val && val->getType());
    t->parent = val->getType();
  } else {
    t->parent = ctx->bases[ctx->findBase(attributes[ATTR_PARENT_FUNCTION])].type;
  }
  if (isClassMember) {
    auto &v = ctx->cache->classes[attributes[ATTR_PARENT_CLASS]]
                  .methods[ctx->cache->reverseIdentifierLookup[stmt->name]];
    bool found = false;
    for (auto &i : v) {
      if (i.name == stmt->name) {
        i.type = t;
        found = true;
        break;
      }
    }
    seqassert(found, "cannot find matching class method for {}", stmt->name);
  }

  t->setSrcInfo(stmt->getSrcInfo());
  t = std::static_pointer_cast<FuncType>(t->generalize(ctx->typecheckLevel));
  LOG_REALIZE("[stmt] added func {}: {} (base={}; parent={})", stmt->name,
              t->toString(), ctx->getBase(), printParents(t->parent));

  ctx->bases[ctx->findBase(attributes[ATTR_PARENT_FUNCTION])]
      .visitedAsts[stmt->name] = {TypecheckItem::Func, t};
  ctx->add(TypecheckItem::Func, stmt->name, t, false, false);
}

void TypecheckVisitor::visit(const ClassStmt *stmt) {
  if (in(stmt->attributes, ATTR_GENERIC)) {
    // bool isStatic = !stmt->attributes[ATTR_GENERIC].empty();
    // auto tp = make_shared<LinkType>(LinkType::Generic, ctx->cache->unboundCount++,
    // 0,
    //                                 nullptr, isStatic);
    // ctx->add(TypecheckItem::Type, stmt->name, tp, true, true, isStatic);
    return;
  }

  if (ctx->findInVisited(stmt->name).second && !in(stmt->attributes, "extend"))
    resultStmt = N<ClassStmt>(stmt->name, vector<Param>(), vector<Param>(),
                              N<SuiteStmt>(), map<string, string>(stmt->attributes));
  else
    resultStmt = N<SuiteStmt>(parseClass(stmt));

  if (in(stmt->attributes, "extend"))
    ctx->extendEtape++;
}

vector<StmtPtr> TypecheckVisitor::parseClass(const ClassStmt *stmt) {
  bool extension = in(stmt->attributes, "extend");

  vector<StmtPtr> stmts;
  stmts.push_back(N<ClassStmt>(stmt->name, vector<Param>(), vector<Param>(),
                               N<SuiteStmt>(), map<string, string>(stmt->attributes)));

  ClassTypePtr ct;
  if (!extension) {
    auto &attributes = const_cast<ClassStmt *>(stmt)->attributes;
    ct = make_shared<ClassType>(stmt->name, stmt->isRecord(), vector<TypePtr>(),
                                vector<Generic>(), nullptr);
    if (in(stmt->attributes, "trait"))
      ct->isTrait = true;
    ct->setSrcInfo(stmt->getSrcInfo());
    auto ctxi =
        make_shared<TypecheckItem>(TypecheckItem::Type, ct, ctx->getBase(), true);
    ctx->add(stmt->name, ctxi);
    ctx->bases[ctx->findBase(attributes[ATTR_PARENT_FUNCTION])]
        .visitedAsts[stmt->name] = {TypecheckItem::Type, ct};

    ct->explicits = parseGenerics(stmt->generics, ctx->typecheckLevel);
    ctx->typecheckLevel++;
    for (auto ai = 0; ai < stmt->args.size(); ai++) {
      // TODO: assert if t is generalized!
      ctx->cache->classes[stmt->name].fields[ai].type =
          transformType(stmt->args[ai].type)
              ->getType()
              ->generalize(ctx->typecheckLevel - 1);
      if (stmt->isRecord())
        ct->args.push_back(ctx->cache->classes[stmt->name].fields[ai].type);
    }
    ctx->typecheckLevel--;

    for (auto &g : stmt->generics) {
      auto val = ctx->find(g.name);
      if (auto g = val->getType()) {
        assert(g && g->getLink() && g->getLink()->kind != types::LinkType::Link);
        if (g->getLink()->kind == LinkType::Unbound)
          g->getLink()->kind = LinkType::Generic;
      }
      ctx->remove(g.name);
    }

    LOG_REALIZE("[class] {} (parent={})", ct->toString(), printParents(ct->parent));
    for (auto &m : ctx->cache->classes[stmt->name].fields)
      LOG_REALIZE("       - member: {}: {}", m.name, m.type->toString());
  }

  return stmts;
}

void TypecheckVisitor::visit(const StarPattern *pat) {
  resultPattern = N<StarPattern>();
  resultPattern->setType(
      forceUnify(pat, ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel)));
}

void TypecheckVisitor::visit(const IntPattern *pat) {
  resultPattern = N<IntPattern>(pat->value);
  resultPattern->setType(forceUnify(pat, ctx->findInternal(".int")));
}

void TypecheckVisitor::visit(const BoolPattern *pat) {
  resultPattern = N<BoolPattern>(pat->value);
  resultPattern->setType(forceUnify(pat, ctx->findInternal(".bool")));
}

void TypecheckVisitor::visit(const StrPattern *pat) {
  resultPattern = N<StrPattern>(pat->value, pat->prefix);
  if (pat->prefix == "s")
    resultPattern->setType(forceUnify(pat, ctx->findInternal(".seq")));
  else
    resultPattern->setType(forceUnify(pat, ctx->findInternal(".str")));
}

void TypecheckVisitor::visit(const RangePattern *pat) {
  resultPattern = N<RangePattern>(pat->start, pat->stop);
  resultPattern->setType(forceUnify(pat, ctx->findInternal(".int")));
}

void TypecheckVisitor::visit(const TuplePattern *pat) {
  auto p = N<TuplePattern>(transform(pat->patterns));
  vector<TypePtr> types;
  for (auto &pp : p->patterns)
    types.push_back(pp->getType());
  auto t = ctx->instantiateGeneric(
      getSrcInfo(), ctx->findInternal(format(".Tuple.{}", types.size())), {types});
  resultPattern = move(p);
  resultPattern->setType(forceUnify(pat, t));
}

void TypecheckVisitor::visit(const ListPattern *pat) {
  auto p = N<ListPattern>(transform(pat->patterns));
  TypePtr t = ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel);
  for (auto &pp : p->patterns)
    forceUnify(t, pp->getType());
  t = ctx->instantiateGeneric(getSrcInfo(), ctx->findInternal(".List"), {t});
  resultPattern = move(p);
  resultPattern->setType(forceUnify(pat, t));
}

void TypecheckVisitor::visit(const OrPattern *pat) {
  auto p = N<OrPattern>(transform(pat->patterns));
  assert(p->patterns.size());
  TypePtr t = p->patterns[0]->getType();
  for (auto &pp : p->patterns)
    forceUnify(t, pp->getType());
  resultPattern = move(p);
  resultPattern->setType(forceUnify(pat, t));
}

void TypecheckVisitor::visit(const WildcardPattern *pat) {
  resultPattern = N<WildcardPattern>(pat->var);
  auto t = forceUnify(pat, ctx->addUnbound(getSrcInfo(), ctx->typecheckLevel));
  if (pat->var != "")
    ctx->add(TypecheckItem::Var, pat->var, t);
  resultPattern->setType(t);
}

void TypecheckVisitor::visit(const GuardedPattern *pat) {
  auto p = N<GuardedPattern>(transform(pat->pattern), transform(pat->cond));
  auto t = p->pattern->getType();
  resultPattern = move(p);
  resultPattern->setType(forceUnify(pat, t));
}

void TypecheckVisitor::visit(const BoundPattern *pat) {
  auto p = N<BoundPattern>(pat->var, transform(pat->pattern));
  auto t = p->pattern->getType();
  ctx->add(TypecheckItem::Var, p->var, t);
  resultPattern = move(p);
  resultPattern->setType(forceUnify(pat, t));
}

/*******************************/

StaticVisitor::StaticVisitor(map<string, types::Generic> &m)
    : generics(m), evaluated(false), value(0) {}

pair<bool, int> StaticVisitor::transform(const ExprPtr &e) {
  StaticVisitor v(generics);
  e->accept(v);
  return {v.evaluated, v.evaluated ? v.value : -1};
}

void StaticVisitor::visit(const IdExpr *expr) {
  auto val = generics.find(expr->value);
  auto t = val->second.type->follow();
  if (t->getLink()) {
    evaluated = false;
  } else {
    assert(t->getStatic() && t->getStatic()->explicits.size() <= 1);
    evaluated = t->canRealize();
    if (evaluated)
      value = t->getStatic()->getValue();
  }
}

void StaticVisitor::visit(const IntExpr *expr) {
  evaluated = true;
  value = std::stoull(expr->value, nullptr, 0);
}

void StaticVisitor::visit(const UnaryExpr *expr) {
  std::tie(evaluated, value) = transform(expr->expr);
  if (evaluated) {
    if (expr->op == "-")
      value = -value;
    else if (expr->op == "!")
      value = !bool(value);
    else
      error(expr->getSrcInfo(), "not a static unary expression");
  }
}

void StaticVisitor::visit(const IfExpr *expr) {
  std::tie(evaluated, value) = transform(expr->cond);
  // Note: both expressions must be evaluated at this time in order to capture
  // all
  //       unrealized variables (i.e. short-circuiting is not possible)
  auto i = transform(expr->ifexpr);
  auto e = transform(expr->elsexpr);
  if (evaluated)
    std::tie(evaluated, value) = value ? i : e;
}

void StaticVisitor::visit(const BinaryExpr *expr) {
  std::tie(evaluated, value) = transform(expr->lexpr);
  bool evaluated2;
  int value2;
  std::tie(evaluated2, value2) = transform(expr->rexpr);
  evaluated &= evaluated2;
  if (!evaluated)
    return;
  if (expr->op == "<")
    value = value < value2;
  else if (expr->op == "<=")
    value = value <= value2;
  else if (expr->op == ">")
    value = value > value2;
  else if (expr->op == ">=")
    value = value >= value2;
  else if (expr->op == "==")
    value = value == value2;
  else if (expr->op == "!=")
    value = value != value2;
  else if (expr->op == "&&")
    value = value && value2;
  else if (expr->op == "||")
    value = value || value2;
  else if (expr->op == "+")
    value = value + value2;
  else if (expr->op == "-")
    value = value - value2;
  else if (expr->op == "*")
    value = value * value2;
  else if (expr->op == "//") {
    if (!value2)
      error("division by zero");
    value = value / value2;
  } else if (expr->op == "%") {
    if (!value2)
      error("division by zero");
    value = value % value2;
  } else
    error(expr->getSrcInfo(), "not a static binary expression");
}

string TypecheckVisitor::patchIfRealizable(TypePtr typ, bool isClass) {
  if (typ->canRealize()) {
    if (isClass) {
      auto r = realizeType(typ->getClass());
      forceUnify(typ, r);
      return r->realizeString();
    } else if (typ->getFunc()) {
      auto r = realizeFunc(typ->getFunc());
      return r->realizeString();
    }
  }
  return "";
}

FuncTypePtr
TypecheckVisitor::findBestCall(ClassTypePtr c, const string &member,
                               const vector<pair<string, types::TypePtr>> &args,
                               bool failOnMultiple, types::TypePtr retType) {
  auto m = ctx->findMethod(c->name, member);
  if (m.empty())
    return nullptr;
  if (m.size() == 1) // works
    return m[0];

  // TODO: For now, overloaded functions are only possible in magic methods
  if (member.substr(0, 2) != "__" || member.substr(member.size() - 2) != "__")
    error("overloaded non-magic method {} in {}", member, c->toString());

  vector<pair<int, int>> scores;
  for (int i = 0; i < m.size(); i++) {
    auto mt = dynamic_pointer_cast<FuncType>(
        ctx->instantiate(getSrcInfo(), m[i], c.get(), false));

    vector<pair<string, TypePtr>> reorderedArgs;
    int s;
    if ((s = reorder(args, reorderedArgs, mt)) == -1)
      continue;

    for (int j = 0; j < reorderedArgs.size(); j++) {
      auto mac = mt->args[j + 1]->getClass();
      if (mac && mac->isTrait) // treat traits as generics
        continue;
      if (!reorderedArgs[j].second) // default arguments don't matter at all
        continue;
      auto ac = reorderedArgs[j].second->getClass();

      Type::Unification us;
      int u = reorderedArgs[j].second->unify(mt->args[j + 1], us);
      us.undo();
      if (u < 0) {
        if (mac && mac->name == ".Optional" && ac && ac->name != mac->name) { // wrap
          int u = reorderedArgs[j].second->unify(mac->explicits[0].type, us);
          us.undo();
          if (u >= 0) {
            s += u + 2;
            continue;
          }
        }
        if (ac && ac->name == ".Optional" && mac && ac->name != mac->name) { // unwrap
          int u = ac->explicits[0].type->unify(mt->args[j + 1], us);
          us.undo();
          if (u >= 0) {
            s += u;
            continue;
          }
        }
        s = -1;
        break;
      } else {
        s += u + 3;
      }
    }
    if (retType) {
      Type::Unification us;
      int u = retType->unify(mt->args[0], us);
      us.undo();
      s = u < 0 ? -1 : s + u;
    }
    if (s >= 0)
      scores.push_back({s, i});
  }
  if (!scores.size()) {
    return nullptr;
  }
  sort(scores.begin(), scores.end(), std::greater<pair<int, int>>());
  if (failOnMultiple) {
    // for (int i = 1; i < scores.size(); i++)
    //   if (scores[i].first == scores[0].first)
    //     // return nullptr;
    //     compilationWarning(format("multiple choices for magic call, selected
    //     {}",
    //                               (*m)[scores[0].second]->canonicalName),
    //                        getSrcInfo().file, getSrcInfo().line);
    //   else
    //     break;
  }
  return m[scores[0].second];
}

vector<types::Generic> TypecheckVisitor::parseGenerics(const vector<Param> &generics,
                                                       int level) {
  auto genericTypes = vector<types::Generic>();
  for (auto &g : generics) {
    assert(!g.name.empty());
    auto tp = ctx->addUnbound(getSrcInfo(), level, true, bool(g.type));
    genericTypes.push_back(
        {g.name, tp->generalize(level), ctx->cache->unboundCount - 1, clone(g.deflt)});
    LOG_REALIZE("[generic] {} -> {} {}", g.name, tp->toString(), bool(g.type));
    ctx->add(TypecheckItem::Type, g.name, tp, true, bool(g.type));
    /*auto tg = ctx->find(g.name)->type;
    assert(tg->getLink() && tg->getLink()->kind == LinkType::Generic);
    genericTypes.emplace_back(
        types::Generic{g.name, tg, tg->getLink()->id, clone(g.deflt)});
    //    LOG_REALIZE("[generic] {} -> {} {}", g.name, tg->toString(), bool(g.type));
    ctx->add(TypecheckItem::Type, g.name,
             make_shared<LinkType>(LinkType::Unbound, tg->getLink()->id, level,
    nullptr, tg->getLink()->isStatic), false, true, tg->getLink()->isStatic);*/
  }
  return genericTypes;
}

void TypecheckVisitor::addFunctionGenerics(FuncTypePtr t) {
  int pi = 0;
  for (auto p = t->parent; p; pi++) {
    if (auto y = p->getFunc()) {
      for (auto &g : y->explicits)
        if (auto s = g.type->getStatic())
          ctx->add(TypecheckItem::Type, g.name, s, false, true);
        else if (!g.name.empty())
          ctx->add(TypecheckItem::Type, g.name, g.type);
      p = y->parent;
    } else {
      auto c = p->getClass();
      assert(c);
      for (auto &g : c->explicits)
        if (auto s = g.type->getStatic())
          ctx->add(TypecheckItem::Type, g.name, s, false, true);
        else if (!g.name.empty())
          ctx->add(TypecheckItem::Type, g.name, g.type);
      p = c->parent;
    }
  }
  for (auto &g : t->explicits)
    if (auto s = g.type->getStatic())
      ctx->add(TypecheckItem::Type, g.name, s, false, true);
    else if (!g.name.empty())
      ctx->add(TypecheckItem::Type, g.name, g.type);
}

types::TypePtr TypecheckVisitor::realizeFunc(types::TypePtr tt) {
  auto t = tt->getFunc();
  assert(t && t->canRealize());
  try {
    auto it = ctx->cache->functions[t->name].realizations.find(t->realizeString());
    if (it != ctx->cache->functions[t->name].realizations.end()) {
      forceUnify(t, it->second.type);
      return it->second.type;
    }

    int depth = 1;
    for (auto p = t->parent; p;) {
      if (auto f = p->getFunc()) {
        depth++;
        p = f->parent;
      } else {
        p = p->getClass()->parent;
      }
    }
    auto oldBases = vector<TypeContext::RealizationBase>(ctx->bases.begin() + depth,
                                                         ctx->bases.end());
    while (ctx->bases.size() > depth)
      ctx->bases.pop_back();

    if (startswith(t->name, ".Tuple.") &&
        (endswith(t->name, ".__iter__") || endswith(t->name, ".__getitem__"))) {
      auto u = t->args[1]->getClass();
      string s;
      for (auto &a : u->args) {
        if (s.empty())
          s = a->realizeString();
        else if (s != a->realizeString())
          error("cannot iterate a heterogenous tuple");
      }
    }

    LOG_TYPECHECK("[realize] fn {} -> {} : base {} ; depth = {}", t->name,
                  t->realizeString(), ctx->getBase(), depth);
    ctx->addBlock();
    ctx->typecheckLevel++;
    ctx->bases.push_back({t->name, t, t->args[0]});
    auto *ast = ctx->cache->functions[t->name].ast.get();
    addFunctionGenerics(t);
    // There is no AST linked to internal functions, so just ignore them
    bool isInternal = in(ast->attributes, ATTR_INTERNAL);
    isInternal |= ast->suite == nullptr;
    if (!isInternal)
      for (int i = 1; i < t->args.size(); i++) {
        assert(t->args[i] && !t->args[i]->getUnbounds().size());
        ctx->add(TypecheckItem::Var, ast->args[i - 1].name,
                 make_shared<LinkType>(t->args[i]));
      }

    // Need to populate funcRealization in advance to make recursive functions
    // viable
    ctx->cache->functions[t->name].realizations[t->realizeString()] = {t, nullptr};
    ctx->bases[0].visitedAsts[t->realizeString()] = {TypecheckItem::Func,
                                                     t}; // realizations go to the top
    // ctx->getRealizations()->realizationLookup[t->realizeString()] = name;

    StmtPtr realized = nullptr;
    if (!isInternal) {
      ctx->typecheckLevel++;
      auto oldIter = ctx->iteration;
      ctx->iteration = 0;
      realized = realizeBlock(ast->suite);
      ctx->iteration = oldIter;
      ctx->typecheckLevel--;

      if (!ast->ret && t->args[0]->getUnbound())
        forceUnify(t->args[0], ctx->findInternal(".void"));
      // if (stmt->)
      // forceUnify(t->args[0], ctx->bases.back().returnType ?
      // ctx->bases.back().returnType : ctx->findInternal(".void"));
    }
    assert(t->args[0]->getClass() && t->args[0]->getClass()->canRealize());
    realizeType(t->args[0]->getClass());
    assert(ast->args.size() == t->args.size() - 1);
    vector<Param> args;
    for (auto &i : ast->args)
      args.emplace_back(Param{i.name, nullptr, nullptr});

    LOG_REALIZE("done with {}", t->realizeString());
    ctx->cache->functions[t->name].realizations[t->realizeString()].ast =
        Nx<FunctionStmt>(ast, t->realizeString(), nullptr, vector<Param>(), move(args),
                         move(realized), map<string, string>(ast->attributes));
    ctx->bases.pop_back();
    ctx->popBlock();
    ctx->typecheckLevel--;

    ctx->bases.insert(ctx->bases.end(), oldBases.begin(), oldBases.end());

    return t;
  } catch (exc::ParserException &e) {
    e.trackRealize(fmt::format("{} (arguments {})", t->name, t->toString()),
                   getSrcInfo());
    throw;
  }
}

types::TypePtr TypecheckVisitor::realizeType(types::TypePtr tt) {
  auto t = tt->getClass();
  assert(t && t->canRealize());
  types::TypePtr ret = nullptr;
  try {
    auto it = ctx->cache->classes[t->name].realizations.find(t->realizeString());
    if (it != ctx->cache->classes[t->name].realizations.end()) {
      ret = it->second.type;
      goto end;
    }

    LOG_REALIZE("[realize] ty {} -> {}", t->name, t->realizeString());
    ctx->bases[0].visitedAsts[t->realizeString()] = {TypecheckItem::Type,
                                                     t}; // realizations go to the top
    ctx->cache->classes[t->name].realizations[t->realizeString()] = {t, {}};
    ret = t;
    goto end;
  } catch (exc::ParserException &e) {
    e.trackRealize(t->toString(), getSrcInfo());
    throw;
  }
end:
  // Check if all members are initialized
  if (ctx->cache->classes[t->name].realizations[t->realizeString()].fields.empty()) {
    for (auto &m : ctx->cache->classes[t->name].fields)
      if (!m.type)
        return ret;
    for (auto &m : ctx->cache->classes[t->name].fields) {
      auto mt = ctx->instantiate(t->getSrcInfo(), m.type, t.get());
      LOG_REALIZE("- member: {} -> {}: {}", m.name, m.type->toString(), mt->toString());
      assert(mt->getClass() && mt->getClass()->canRealize());
      ctx->cache->classes[t->name].realizations[t->realizeString()].fields.emplace_back(
          m.name, realizeType(mt->getClass()));
    }
  }
  return ret;
}

StmtPtr TypecheckVisitor::realizeBlock(const StmtPtr &stmt, bool keepLast) {
  if (!stmt)
    return nullptr;
  StmtPtr result = nullptr;

  // We keep running typecheck transformations until there are no more unbound
  // types. It is assumed that the unbound count will decrease in each
  // iteration--- if not, the program cannot be type-checked.
  // TODO: this can be probably optimized one day...
  int minUnbound = ctx->cache->unboundCount;
  for (int iter = 0, prevSize = INT_MAX;; iter++, ctx->iteration++) {
    ctx->addBlock();
    if (keepLast) // reset extendCount in whole code loop
      ctx->extendEtape = 0;
    result = TypecheckVisitor(ctx).transform(result ? result : stmt);

    int newUnbounds = 0;
    set<types::TypePtr> newActiveUnbounds;
    for (auto i = ctx->activeUnbounds.begin(); i != ctx->activeUnbounds.end();) {
      auto l = (*i)->getLink();
      assert(l);
      if (l->kind == LinkType::Unbound) {
        newActiveUnbounds.insert(*i);
        if (l->id >= minUnbound)
          newUnbounds++;
      }
      ++i;
    }
    ctx->activeUnbounds = newActiveUnbounds;

    ctx->popBlock();
    if (ctx->activeUnbounds.empty() || !newUnbounds) {
      break;
    } else {

      if (newUnbounds >= prevSize) {
        TypePtr fu = nullptr;
        int count = 0;
        for (auto &ub : ctx->activeUnbounds)
          if (ub->getLink()->id >= minUnbound) {
            // Attempt to use default generics here
            // TODO: this is awfully inefficient way to do it
            // if (ctx->...)
            if (!fu)
              fu = ub;
            LOG_TYPECHECK("[realizeBlock] dangling {} @ {}", ub->toString(),
                          ub->getSrcInfo());
            count++;
          }
        error(fu, "cannot resolve {} unbound variables", count);
      }
      prevSize = newUnbounds;
    }
    LOG_TYPECHECK("=========================== {}",
                  ctx->bases.back().type ? ctx->bases.back().type->toString() : "-");
  }
  // Last pass; TODO: detect if it is needed...
  ctx->addBlock();
  LOG_TYPECHECK("=========================== {}",
                ctx->bases.back().type ? ctx->bases.back().type->toString() : "-");
  result = TypecheckVisitor(ctx).transform(result);
  if (!keepLast)
    ctx->popBlock();
  return result;
}

int TypecheckVisitor::reorder(const vector<pair<string, TypePtr>> &args,
                              vector<pair<string, TypePtr>> &reorderedArgs,
                              types::FuncTypePtr f) {
  vector<int> argIndex;
  for (int i = 0; i < int(f->args.size()) - 1; i++)
    argIndex.push_back(i);
  string knownTypes;

  bool namesStarted = false;
  unordered_map<string, TypePtr> namedArgs;
  for (int i = 0; i < args.size(); i++) {
    if (args[i].first == "" && namesStarted)
      error("unnamed argument after a named argument");
    namesStarted |= args[i].first != "";
    if (args[i].first == "")
      reorderedArgs.push_back({"", args[i].second});
    else if (namedArgs.find(args[i].first) == namedArgs.end())
      namedArgs[args[i].first] = args[i].second;
    else
      return -1;
  }

  if (reorderedArgs.size() + namedArgs.size() != argIndex.size())
    return -1;

  int score = reorderedArgs.size() * 2;

  FunctionStmt *ast = ctx->cache->functions[f->name].ast.get();
  seqassert(ast, "AST not accessible for {}", f->name);
  for (int i = 0, ra = reorderedArgs.size(); i < argIndex.size(); i++) {
    if (i >= ra) {
      assert(ast);
      auto it = namedArgs.find(ast->args[argIndex[i]].name);
      if (it != namedArgs.end()) {
        reorderedArgs.push_back({"", it->second});
        namedArgs.erase(it);
        score += 2;
      } else if (ast->args[i].deflt) {
        if (ast->args[argIndex[i]].type) {
          reorderedArgs.push_back({"", f->args[argIndex[i] + 1]});
        } else { // TODO: does this even work? any dangling issues?
          // auto t = simplify(tmp->args[argIndex[i]].deflt);
          reorderedArgs.push_back({"", nullptr}); // really does not matter
        }
        score += 1;
      } else {
        return -1;
      }
    }
  }
  return score;
}

string TypecheckVisitor::generatePartialStub(const string &mask,
                                             const string &oldMask) {
  auto typeName = fmt::format(".Partial.{}", mask);
  if (ctx->cache->variardics.find(typeName) == ctx->cache->variardics.end()) {
    ctx->cache->variardics.insert(typeName);

    vector<Param> generics, args, missingArgs;
    vector<ExprPtr> genericNames, callArgs;
    args.emplace_back(Param{".ptr", nullptr, nullptr});
    missingArgs.push_back(Param{"self", nullptr, nullptr});
    for (int i = 0; i <= mask.size(); i++) {
      genericNames.push_back(N<IdExpr>(format("T{}", i)));
      generics.push_back(Param{format("T{}", i), nullptr, nullptr});
      if (i && mask[i - 1] == '1') {
        args.push_back(Param{format(".a{0}", i), N<IdExpr>(format("T{}", i)), nullptr});
        callArgs.push_back(N<DotExpr>(N<IdExpr>("self"), format(".a{0}", i)));
      } else if (i && mask[i - 1] == '0') {
        missingArgs.push_back(
            Param{format("a{0}", i), N<IdExpr>(format("T{}", i)), nullptr});
        callArgs.push_back(N<IdExpr>(format("a{0}", i)));
      }
    }
    args[0].type = N<IndexExpr>(N<IdExpr>(format("Function", mask.size())),
                                N<TupleExpr>(move(genericNames)));
    StmtPtr func =
        N<FunctionStmt>("__call__", N<IdExpr>("T0"), vector<Param>{}, move(missingArgs),
                        N<ReturnStmt>(N<CallExpr>(N<DotExpr>(N<IdExpr>("self"), ".ptr"),
                                                  move(callArgs))),
                        vector<string>{});
    StmtPtr stmt = make_unique<ClassStmt>(
        typeName, move(generics), move(args), move(func),
        vector<string>{ATTR_TUPLE, "no_total_ordering", "no_pickle", "no_container",
                       "no_python"});

    stmt = SimplifyVisitor::apply(ctx->cache->imports[STDLIB_IMPORT].ctx, stmt,
                                  FILE_GENERATED);
    stmt = TypecheckVisitor(ctx).transform(stmt);
    prependStmts->push_back(move(stmt));
  }

  if (oldMask.empty())
    return typeName + ".__new__";

  auto fnName = format("{}.__new_{}_{}__", typeName, oldMask, mask);
  if (!ctx->find(fnName)) {
    vector<Param> args;
    vector<ExprPtr> newArgs;
    args.push_back(Param{"p", nullptr, nullptr});
    newArgs.push_back(N<DotExpr>(N<IdExpr>("p"), ".ptr"));
    for (int i = 0; i < mask.size(); i++) {
      if (mask[i] == '1' && oldMask[i] == '0') {
        args.push_back(Param{format("a{}", i), nullptr, nullptr});
        newArgs.push_back(N<IdExpr>(format("a{}", i)));
      } else if (oldMask[i] == '1') {
        newArgs.push_back(N<DotExpr>(N<IdExpr>("p"), format(".a{}", i + 1)));
      }
    }
    ExprPtr callee = N<DotExpr>(N<IdExpr>(typeName), "__new__");
    StmtPtr stmt = make_unique<FunctionStmt>(
        fnName, nullptr, vector<Param>{}, move(args),
        N<SuiteStmt>(N<ReturnStmt>(N<CallExpr>(move(callee), move(newArgs)))),
        vector<string>{});
    stmt = SimplifyVisitor::apply(ctx->cache->imports[STDLIB_IMPORT].ctx, stmt,
                                  FILE_GENERATED);
    stmt = TypecheckVisitor(ctx).transform(stmt);
    prependStmts->push_back(move(stmt));
  }
  return fnName;
}

} // namespace ast
} // namespace seq
