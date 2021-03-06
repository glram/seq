/**
 * codegen.h
 * Code generation AST walker.
 *
 * Transforms a given AST to a Seq LLVM AST.
 */

#pragma once

#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "lang/seq.h"
#include "parser/ast/ast.h"
#include "parser/ast/codegen_ctx.h"
#include "parser/ast/visitor.h"
#include "parser/ast/walk.h"
#include "parser/common.h"

namespace seq {
namespace ast {

class CodegenVisitor : public ASTVisitor, public SrcObject {
  std::shared_ptr<LLVMContext> ctx;
  seq::Expr *resultExpr;
  seq::Stmt *resultStmt;
  seq::Pattern *resultPattern;

  void defaultVisit(const Expr *expr) override;
  void defaultVisit(const Stmt *expr) override;
  void defaultVisit(const Pattern *expr) override;

  seq::BaseFunc *realizeFunc(types::FuncTypePtr t);
  seq::types::Type *realizeType(types::ClassTypePtr t);

  std::shared_ptr<LLVMItem::Item>
  processIdentifier(std::shared_ptr<LLVMContext> tctx, const std::string &id);

public:
  CodegenVisitor(std::shared_ptr<LLVMContext> ctx);

  seq::Expr *transform(const Expr *expr);
  seq::Stmt *transform(const Stmt *stmt);
  seq::Pattern *transform(const Pattern *pat);

public:
  void visit(const BoolExpr *) override;
  void visit(const IntExpr *) override;
  void visit(const FloatExpr *) override;
  void visit(const StringExpr *) override;
  void visit(const IdExpr *) override;
  void visit(const TupleExpr *) override;
  void visit(const IfExpr *) override;
  void visit(const UnaryExpr *) override;
  void visit(const BinaryExpr *) override;
  void visit(const PipeExpr *) override;
  void visit(const IndexExpr *) override;
  void visit(const CallExpr *) override;
  void visit(const DotExpr *) override;
  void visit(const EllipsisExpr *) override;
  void visit(const PtrExpr *) override;
  void visit(const YieldExpr *) override;

  void visit(const SuiteStmt *) override;
  void visit(const PassStmt *) override;
  void visit(const BreakStmt *) override;
  void visit(const ContinueStmt *) override;
  void visit(const ExprStmt *) override;
  void visit(const AssignStmt *) override;
  void visit(const DelStmt *) override;
  void visit(const PrintStmt *) override;
  void visit(const ReturnStmt *) override;
  void visit(const YieldStmt *) override;
  void visit(const AssertStmt *) override;
  void visit(const WhileStmt *) override;
  void visit(const ForStmt *) override;
  void visit(const IfStmt *) override;
  void visit(const MatchStmt *) override;
  void visit(const ImportStmt *) override;
  void visit(const ExternImportStmt *) override;
  void visit(const TryStmt *) override;
  void visit(const GlobalStmt *) override;
  void visit(const ThrowStmt *) override;
  void visit(const FunctionStmt *) override;

  void visit(const StarPattern *) override;
  void visit(const IntPattern *) override;
  void visit(const BoolPattern *) override;
  void visit(const StrPattern *) override;
  void visit(const SeqPattern *) override;
  void visit(const RangePattern *) override;
  void visit(const TuplePattern *) override;
  void visit(const ListPattern *) override;
  void visit(const OrPattern *) override;
  void visit(const WildcardPattern *) override;
  void visit(const GuardedPattern *) override;

private:
  template <typename Tn, typename... Ts> auto N(Ts &&... args) {
    return new Tn(std::forward<Ts>(args)...);
  }
  template <typename T, typename... Ts>
  auto transform(const std::unique_ptr<T> &t, Ts &&... args)
      -> decltype(transform(t.get())) {
    return transform(t.get(), std::forward<Ts>(args)...);
  }
  template <typename T> auto transform(const std::vector<T> &ts) {
    std::vector<T> r;
    for (auto &e : ts)
      r.push_back(transform(e));
    return r;
  }
  template <typename... TArgs>
  void internalError(const char *format, TArgs &&... args) {
    throw exc::ParserException(fmt::format(
        "INTERNAL: {}", fmt::format(format, args...), getSrcInfo()));
  }
};

} // namespace ast
} // namespace seq
