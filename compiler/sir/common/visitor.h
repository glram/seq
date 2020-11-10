#pragma once

#include <memory>

#include "sir/bblock.h"
#include "sir/func.h"
#include "sir/instr.h"
#include "sir/lvalue.h"
#include "sir/module.h"
#include "sir/operand.h"
#include "sir/pattern.h"
#include "sir/rvalue.h"
#include "sir/terminator.h"
#include "sir/trycatch.h"
#include "sir/var.h"

#include "util/fmt/format.h"

#define DEFAULT_VISIT(x)                                                               \
  virtual void visit(std::shared_ptr<x> n) {                                           \
    throw std::runtime_error("cannot visit node");                                     \
  }

namespace seq {
namespace ir {

namespace common {
using namespace seq::ir;

class LLVMOperand;

/// Base for SIR visitors
class SIRVisitor {
public:
  DEFAULT_VISIT(SIRModule);

  DEFAULT_VISIT(BasicBlock);

  DEFAULT_VISIT(TryCatch);

  DEFAULT_VISIT(Func);
  DEFAULT_VISIT(Var);

  DEFAULT_VISIT(Instr);
  DEFAULT_VISIT(AssignInstr);
  DEFAULT_VISIT(RvalueInstr);

  DEFAULT_VISIT(Rvalue);
  DEFAULT_VISIT(MemberRvalue);
  DEFAULT_VISIT(CallRvalue);
  DEFAULT_VISIT(PartialCallRvalue);
  DEFAULT_VISIT(OperandRvalue);
  DEFAULT_VISIT(MatchRvalue);
  DEFAULT_VISIT(PipelineRvalue);
  DEFAULT_VISIT(StackAllocRvalue);

  DEFAULT_VISIT(Lvalue);
  DEFAULT_VISIT(VarLvalue);
  DEFAULT_VISIT(VarMemberLvalue);

  DEFAULT_VISIT(Operand);
  DEFAULT_VISIT(VarOperand);
  DEFAULT_VISIT(VarPointerOperand);
  DEFAULT_VISIT(LiteralOperand);
  DEFAULT_VISIT(LLVMOperand);

  DEFAULT_VISIT(Pattern);
  DEFAULT_VISIT(WildcardPattern);
  DEFAULT_VISIT(BoundPattern);
  DEFAULT_VISIT(StarPattern);
  DEFAULT_VISIT(IntPattern);
  DEFAULT_VISIT(BoolPattern);
  DEFAULT_VISIT(StrPattern);
  DEFAULT_VISIT(SeqPattern);
  DEFAULT_VISIT(RecordPattern);
  DEFAULT_VISIT(ArrayPattern);
  DEFAULT_VISIT(OptionalPattern);
  DEFAULT_VISIT(RangePattern);
  DEFAULT_VISIT(OrPattern);
  DEFAULT_VISIT(GuardedPattern);

  DEFAULT_VISIT(Terminator);
  DEFAULT_VISIT(JumpTerminator);
  DEFAULT_VISIT(CondJumpTerminator);
  DEFAULT_VISIT(ReturnTerminator);
  DEFAULT_VISIT(YieldTerminator);
  DEFAULT_VISIT(ThrowTerminator);
  DEFAULT_VISIT(AssertTerminator);
  DEFAULT_VISIT(FinallyTerminator);

  DEFAULT_VISIT(types::Type);
  DEFAULT_VISIT(types::RecordType);
  DEFAULT_VISIT(types::RefType);
  DEFAULT_VISIT(types::FuncType);
  DEFAULT_VISIT(types::PartialFuncType);
  DEFAULT_VISIT(types::OptionalType);
  DEFAULT_VISIT(types::ArrayType);
  DEFAULT_VISIT(types::PointerType);
  DEFAULT_VISIT(types::GeneratorType);
  DEFAULT_VISIT(types::IntNType);
};

/// Special operand wrapping LLVM values
class LLVMOperand : public Operand {
private:
  /// operand type
  std::shared_ptr<types::Type> type;
  /// value
  llvm::Value *val;

  std::shared_ptr<types::Type> opType() override { return type; }

public:
  /// Constructs an llvm operand.
  /// @param type the type
  /// @param val the internal value
  LLVMOperand(std::shared_ptr<types::Type> type, llvm::Value *val)
      : type(std::move(type)), val(val) {}

  void accept(common::SIRVisitor &v) override { v.visit(getShared<LLVMOperand>()); }

  /// @return thh value
  llvm::Value *getValue() { return val; }

  std::string textRepresentation() const override { return "internal"; }
};

/// CRTP Base for SIR visitors that return from transform.
template <typename Derived, typename Context, typename ModuleResult,
          typename BlockResult, typename TCResult, typename VarResult,
          typename InstrResult, typename RvalResult, typename LvalResult,
          typename OpResult, typename PatResult, typename TermResult,
          typename TypeResult>
class CallbackIRVisitor : public SIRVisitor {
protected:
  std::shared_ptr<Context> ctx;
  ModuleResult moduleResult{};
  BlockResult blockResult{};
  TCResult tcResult{};
  VarResult varResult{};
  InstrResult instrResult{};
  RvalResult rvalResult{};
  LvalResult lvalResult{};
  OpResult opResult{};
  PatResult patResult{};
  TermResult termResult{};
  TypeResult typeResult{};

public:
  explicit CallbackIRVisitor(std::shared_ptr<Context> ctx) : ctx(std::move(ctx)) {}

  template <typename... Args>
  ModuleResult transform(std::shared_ptr<ir::SIRModule> module, Args... args) {
    Derived v(ctx, args...);
    module->accept(v);
    return v.moduleResult;
  }
  template <typename... Args>
  BlockResult transform(std::shared_ptr<ir::BasicBlock> block, Args... args) {
    Derived v(ctx, args...);
    block->accept(v);
    return v.blockResult;
  }
  template <typename... Args>
  TCResult transform(std::shared_ptr<ir::TryCatch> tc, Args... args) {
    Derived v(ctx, args...);
    tc->accept(v);
    return v.tcResult;
  }
  template <typename... Args>
  VarResult transform(std::shared_ptr<ir::Var> var, Args... args) {
    Derived v(ctx, args...);
    var->accept(v);
    return v.varResult;
  }
  template <typename... Args>
  InstrResult transform(std::shared_ptr<ir::Instr> instr, Args... args) {
    Derived v(ctx, args...);
    instr->accept(v);
    return v.instrResult;
  }
  template <typename... Args>
  RvalResult transform(std::shared_ptr<ir::Rvalue> rval, Args... args) {
    Derived v(ctx, args...);
    rval->accept(v);
    return v.rvalResult;
  }
  template <typename... Args>
  LvalResult transform(std::shared_ptr<Lvalue> lval, Args... args) {
    Derived v(ctx, args...);
    lval->accept(v);
    return v.lvalResult;
  }
  template <typename... Args>
  OpResult transform(std::shared_ptr<Operand> op, Args... args) {
    Derived v(ctx, args...);
    op->accept(v);
    return v.opResult;
  }
  template <typename... Args>
  PatResult transform(std::shared_ptr<Pattern> pat, Args... args) {
    Derived v(ctx, args...);
    pat->accept(v);
    return v.patResult;
  }
  template <typename... Args>
  TermResult transform(std::shared_ptr<Terminator> term, Args... args) {
    Derived v(ctx, args...);
    term->accept(v);
    return v.termResult;
  }
  template <typename... Args>
  TypeResult transform(std::shared_ptr<types::Type> typ, Args... args) {
    Derived v(ctx, args...);
    typ->accept(v);
    return v.typeResult;
  }
};

} // namespace common
} // namespace ir
} // namespace seq

#undef DEFAULT_VISIT