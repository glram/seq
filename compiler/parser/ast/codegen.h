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
#include "sir/base.h"
#include "sir/pattern.h"

namespace seq {
namespace ast {

struct CodegenResult {
  enum { OP, RVALUE, LVALUE, PATTERN, NONE } tag;
  std::shared_ptr<seq::ir::Operand> operandResult;
  std::shared_ptr<seq::ir::Rvalue> rvalueResult;
  std::shared_ptr<seq::ir::Lvalue> lvalueResult;
  std::shared_ptr<seq::ir::Pattern> patternResult;

  CodegenResult()
      : tag(NONE), operandResult(nullptr), rvalueResult(nullptr),
        lvalueResult(nullptr), patternResult(nullptr){};
  explicit CodegenResult(std::shared_ptr<seq::ir::Operand> op)
      : tag(OP), operandResult(op), rvalueResult(nullptr),
        lvalueResult(nullptr), patternResult(nullptr){};
  explicit CodegenResult(std::shared_ptr<seq::ir::Rvalue> rval)
      : tag(RVALUE), operandResult(nullptr), rvalueResult(rval),
        lvalueResult(nullptr), patternResult(nullptr){};
  explicit CodegenResult(std::shared_ptr<seq::ir::Lvalue> lval)
      : tag(LVALUE), operandResult(nullptr), rvalueResult(nullptr),
        lvalueResult(lval), patternResult(nullptr){};
  explicit CodegenResult(std::shared_ptr<seq::ir::Pattern> pattern)
      : tag(PATTERN), operandResult(nullptr), rvalueResult(nullptr),
        lvalueResult(nullptr), patternResult(pattern){};

  void addAttribute(std::string key, std::shared_ptr<seq::ir::Attribute> att) {
    switch (tag) {
    case OP:
      operandResult->setAttribute(key, att);
      break;
    case RVALUE:
      rvalueResult->setAttribute(key, att);
      break;
    case LVALUE:
      lvalueResult->setAttribute(key, att);
      break;
    case PATTERN:
      patternResult->setAttribute(key, att);
      break;
    }
  }
};

class CodegenVisitor : public ASTVisitor, public SrcObject {
  std::shared_ptr<LLVMContext> ctx;
  CodegenResult result;

  void defaultVisit(const Expr *expr) override;
  void defaultVisit(const Stmt *expr) override;
  void defaultVisit(const Pattern *expr) override;

  seq::ir::types::Type *realizeType(types::ClassTypePtr t);

  std::shared_ptr<LLVMItem::Item>
  processIdentifier(std::shared_ptr<LLVMContext> tctx, const std::string &id);

public:
  CodegenVisitor(std::shared_ptr<LLVMContext> ctx);

  CodegenResult transform(const Expr *expr);
  CodegenResult transform(const Stmt *stmt);
  CodegenResult transform(const Pattern *pat);

  void visitMethods(const std::string &name);
  CodegenResult flatten(const CodegenResult res);

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
  void visit(const TupleIndexExpr *) override;
  void visit(const CallExpr *) override;
  void visit(const StackAllocExpr *) override;
  void visit(const DotExpr *) override;
  // void visit(const EllipsisExpr *) override;
  void visit(const PtrExpr *) override;
  void visit(const YieldExpr *) override;

  void visit(const SuiteStmt *) override;
  void visit(const PassStmt *) override;
  void visit(const BreakStmt *) override;
  void visit(const ContinueStmt *) override;
  void visit(const ExprStmt *) override;
  void visit(const AssignStmt *) override;
  void visit(const AssignMemberStmt *) override;
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
  void visit(const UpdateStmt *) override;
  void visit(const TryStmt *) override;
  void visit(const GlobalStmt *) override;
  void visit(const ThrowStmt *) override;
  void visit(const FunctionStmt *) override;
  void visit(const ClassStmt *stmt) override;

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
  template <typename A, typename B, typename... Ts> auto Nas(Ts &&... args) {
    return std::static_pointer_cast<B>(
        std::make_shared<A>(std::forward<Ts>(args)...));
  }

  template <typename A, typename... Ts> auto Ns(Ts &&... args) {
    return std::make_shared<A>(std::forward<Ts>(args)...);
  }
};

} // namespace ast
} // namespace seq
