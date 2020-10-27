#pragma once

#include <memory>

#include "context.h"

#include "sir/common/visitor.h"

#define OVERRIDE_VISIT(x) void visit(std::shared_ptr<x>) override
#define NORMAL_VISIT(x) void visit(std::shared_ptr<x>)

namespace seq {
namespace ir {
namespace codegen {

class CodegenVisitor;

using InlinePatternCheckFunc = std::function<llvm::Value *(llvm::Value *)>;
using Parent =
    common::CallbackIRVisitor<CodegenVisitor, Context, llvm::Module *,
                              llvm::BasicBlock *, std::shared_ptr<TryCatchMetadata>,
                              llvm::Value *, llvm::Instruction *, llvm::Value *,
                              llvm::Value *, llvm::Value *, InlinePatternCheckFunc,
                              llvm::Instruction *, std::shared_ptr<TypeRealization>>;

class CodegenVisitor : public Parent {
private:
  std::string nameOverride;

public:
  explicit CodegenVisitor(std::shared_ptr<Context> ctx, std::string nameOverride = "")
      : Parent(std::move(ctx)), nameOverride(std::move(nameOverride)) {}

  using Parent::visit;

  OVERRIDE_VISIT(IRModule);

  OVERRIDE_VISIT(BasicBlock);

  OVERRIDE_VISIT(TryCatch);

  OVERRIDE_VISIT(Func);
  OVERRIDE_VISIT(Var);

  OVERRIDE_VISIT(AssignInstr);
  OVERRIDE_VISIT(RvalueInstr);

  OVERRIDE_VISIT(MemberRvalue);
  OVERRIDE_VISIT(CallRvalue);
  OVERRIDE_VISIT(PartialCallRvalue);
  OVERRIDE_VISIT(OperandRvalue);
  OVERRIDE_VISIT(MatchRvalue);
  OVERRIDE_VISIT(PipelineRvalue);
  OVERRIDE_VISIT(StackAllocRvalue);

  OVERRIDE_VISIT(VarLvalue);
  OVERRIDE_VISIT(VarMemberLvalue);

  OVERRIDE_VISIT(VarOperand);
  OVERRIDE_VISIT(VarPointerOperand);
  OVERRIDE_VISIT(LiteralOperand);
  OVERRIDE_VISIT(common::LLVMOperand);

  OVERRIDE_VISIT(WildcardPattern);
  OVERRIDE_VISIT(BoundPattern);
  OVERRIDE_VISIT(StarPattern);
  OVERRIDE_VISIT(IntPattern);
  OVERRIDE_VISIT(BoolPattern);
  OVERRIDE_VISIT(StrPattern);
  OVERRIDE_VISIT(SeqPattern);
  OVERRIDE_VISIT(RecordPattern);
  OVERRIDE_VISIT(ArrayPattern);
  OVERRIDE_VISIT(RangePattern);
  OVERRIDE_VISIT(OrPattern);
  OVERRIDE_VISIT(GuardedPattern);

  OVERRIDE_VISIT(JumpTerminator);
  OVERRIDE_VISIT(CondJumpTerminator);
  OVERRIDE_VISIT(ReturnTerminator);
  OVERRIDE_VISIT(YieldTerminator);
  OVERRIDE_VISIT(ThrowTerminator);
  OVERRIDE_VISIT(AssertTerminator);
  OVERRIDE_VISIT(FinallyTerminator);

  OVERRIDE_VISIT(types::Type);
  OVERRIDE_VISIT(types::RecordType);
  OVERRIDE_VISIT(types::RefType);
  OVERRIDE_VISIT(types::FuncType);
  OVERRIDE_VISIT(types::PartialFuncType);
  OVERRIDE_VISIT(types::Optional);
  OVERRIDE_VISIT(types::Array);
  OVERRIDE_VISIT(types::Pointer);
  OVERRIDE_VISIT(types::Generator);
  OVERRIDE_VISIT(types::IntNType);

private:
  void codegenTcJump(llvm::BasicBlock *dst,
                     std::vector<std::shared_ptr<TryCatch>> tcPath);
};

} // namespace codegen
} // namespace ir
} // namespace seq

#undef OVERRIDE_VISIT
#undef NORMAL_VISIT
