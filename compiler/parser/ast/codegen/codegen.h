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
#include <utility>
#include <vector>

#include "sir/lvalue.h"
#include "sir/module.h"
#include "sir/operand.h"
#include "sir/pattern.h"
#include "sir/rvalue.h"

#include "sir/types/types.h"

#include "parser/ast/ast.h"
#include "parser/ast/cache.h"
#include "parser/ast/codegen/codegen_ctx.h"
#include "parser/ast/visitor.h"
#include "parser/common.h"

namespace seq {
namespace ast {

struct CodegenResult {
  enum { OP, RVALUE, LVALUE, PATTERN, TYPE, NONE } tag;
  std::shared_ptr<seq::ir::Operand> operandResult;
  std::shared_ptr<seq::ir::Rvalue> rvalueResult;
  std::shared_ptr<seq::ir::Lvalue> lvalueResult;
  std::shared_ptr<seq::ir::Pattern> patternResult;
  std::shared_ptr<seq::ir::types::Type> typeResult;

  std::shared_ptr<seq::ir::types::Type> typeOverride;

  CodegenResult() : tag(NONE){};
  explicit CodegenResult(std::shared_ptr<seq::ir::Operand> op)
      : tag(OP), operandResult(std::move(op)){};
  explicit CodegenResult(std::shared_ptr<seq::ir::Rvalue> rval)
      : tag(RVALUE), rvalueResult(std::move(rval)){};
  explicit CodegenResult(std::shared_ptr<seq::ir::Lvalue> lval)
      : tag(LVALUE), lvalueResult(std::move(lval)){};
  explicit CodegenResult(std::shared_ptr<seq::ir::Pattern> pattern)
      : tag(PATTERN), patternResult(std::move(pattern)){};
  explicit CodegenResult(std::shared_ptr<seq::ir::types::Type> type)
      : tag(TYPE), typeResult(std::move(type)){};

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
    default:
      break;
    }
  }
};

class CodegenVisitor
    : public CallbackASTVisitor<CodegenResult, CodegenResult, CodegenResult> {
  std::shared_ptr<CodegenContext> ctx;
  CodegenResult result;

  void defaultVisit(const Expr *expr) override;
  void defaultVisit(const Stmt *expr) override;
  void defaultVisit(const Pattern *expr) override;

  std::shared_ptr<seq::ir::types::Type> realizeType(types::ClassTypePtr t);
  std::shared_ptr<seq::ir::Func> realizeFunc(const std::string &name);
  std::shared_ptr<CodegenItem> processIdentifier(std::shared_ptr<CodegenContext> tctx,
                                                 const std::string &id);

  std::shared_ptr<seq::ir::Operand> toOperand(const CodegenResult res);
  std::shared_ptr<seq::ir::Rvalue> toRvalue(const CodegenResult res);
  std::shared_ptr<seq::ir::BasicBlock> newBlock();
  void condSetTerminator(std::shared_ptr<seq::ir::Terminator> term);

public:
  explicit CodegenVisitor(std::shared_ptr<CodegenContext> ctx);
  static std::shared_ptr<seq::ir::IRModule> apply(std::shared_ptr<Cache> cache,
                                                  StmtPtr stmts);

  CodegenResult transform(const ExprPtr &expr) override;
  CodegenResult transform(const StmtPtr &stmt) override;
  CodegenResult transform(const PatternPtr &pat) override;

  void visitMethods(const std::string &name);

public:
  void visit(const BoolExpr *) override;
  void visit(const IntExpr *) override;
  void visit(const FloatExpr *) override;
  void visit(const StringExpr *) override;
  void visit(const IdExpr *) override;
  void visit(const IfExpr *) override;
  void visit(const PipeExpr *) override;
  void visit(const CallExpr *) override;
  void visit(const StackAllocExpr *) override;
  void visit(const DotExpr *) override;
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
  void visit(const ReturnStmt *) override;
  void visit(const YieldStmt *) override;
  void visit(const AssertStmt *) override;
  void visit(const WhileStmt *) override;
  void visit(const ForStmt *) override;
  void visit(const IfStmt *) override;
  void visit(const MatchStmt *) override;
  void visit(const UpdateStmt *) override;
  void visit(const TryStmt *) override;
  // void visit(const GlobalStmt *) override;
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
};

} // namespace ast
} // namespace seq
