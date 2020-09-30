#pragma once

#include <memory>

#include "context.h"

#define NODE_VISIT(x) void visit(std::shared_ptr<x>)
#define DEFAULT_VISIT(x)                                                               \
  void visit(std::shared_ptr<x>) {}

namespace seq {
namespace ir {

class IRModule;

class BasicBlock;

class Func;
class Var;

class Instr;
class AssignInstr;
class RvalueInstr;

class Rvalue;
class MemberRvalue;
class CallRvalue;
class PartialCallRvalue;
class OperandRvalue;
class MatchRvalue;
class PipelineRvalue;
class StackAllocRvalue;

class Lvalue;
class VarLvalue;
class VarMemberLvalue;

class Operand;
class VarOperand;
class VarPointerOperand;
class LiteralOperand;

class Pattern;
class WildcardPattern;
class BoundPattern;
class StarPattern;
class IntPattern;
class BoolPattern;
class StrPattern;
class SeqPattern;
class RecordPattern;
class ArrayPattern;
class OptionalPattern;
class RangePattern;
class OrPattern;
class GuardedPattern;

class Terminator;
class JumpTerminator;
class CondJumpTerminator;
class ReturnTerminator;
class YieldTerminator;
class ThrowTerminator;
class AssertTerminator;

namespace types {
class Type;
class RecordType;
class RefType;
class FuncType;
class PartialFuncType;
class Optional;
class Array;
class Pointer;
class Generator;
class IntNType;
} // namespace types

namespace codegen {

class CodegenVisitor {
private:
  std::shared_ptr<Context> ctx;
  llvm::Value *valResult;
  llvm::Type *typeResult;

  llvm::Value *alloc(std::shared_ptr<types::Type> type, llvm::Value *count,
                     llvm::IRBuilder<> &builder);
  llvm::Value *callBuiltin(const std::string &signature,
                           std::vector<llvm::Value *> args, llvm::IRBuilder<> &builder);
  llvm::Value *callMagic(std::shared_ptr<types::Type> type,
                         const std::string &signature, std::vector<llvm::Value *> args,
                         llvm::IRBuilder<> &builder);
  llvm::Value *codegenStr(llvm::Value *self, const std::string &name,
                          llvm::BasicBlock *block);

public:
  explicit CodegenVisitor(std::shared_ptr<Context> ctx)
      : ctx(std::move(ctx)), valResult(), typeResult() {}

  void transform(std::shared_ptr<IRModule> module);
  void transform(std::shared_ptr<BasicBlock> block);
  llvm::Value *transform(std::shared_ptr<Var> var,
                         const std::string &nameOverride = "");
  void transform(std::shared_ptr<Instr> instr);
  llvm::Value *transform(std::shared_ptr<Rvalue> rval);
  llvm::Value *transform(std::shared_ptr<Lvalue> lval);
  llvm::Value *transform(std::shared_ptr<Operand> op);
  llvm::Value *transform(std::shared_ptr<Pattern> pat);
  void transform(std::shared_ptr<Terminator> term);
  llvm::Type *transform(std::shared_ptr<types::Type> typ);

  NODE_VISIT(IRModule);

  NODE_VISIT(BasicBlock);

  void visit(std::shared_ptr<Func>, const std::string &);
  void visit(std::shared_ptr<Var>, const std::string &);

  DEFAULT_VISIT(Instr);
  NODE_VISIT(AssignInstr);
  NODE_VISIT(RvalueInstr);

  DEFAULT_VISIT(Rvalue);
  NODE_VISIT(MemberRvalue);
  NODE_VISIT(CallRvalue);
  NODE_VISIT(PartialCallRvalue);
  NODE_VISIT(OperandRvalue);
  NODE_VISIT(MatchRvalue);
  NODE_VISIT(PipelineRvalue);
  NODE_VISIT(StackAllocRvalue);

  DEFAULT_VISIT(Lvalue);
  NODE_VISIT(VarLvalue);
  NODE_VISIT(VarMemberLvalue);

  DEFAULT_VISIT(Operand);
  NODE_VISIT(VarOperand);
  NODE_VISIT(VarPointerOperand);
  NODE_VISIT(LiteralOperand);

  DEFAULT_VISIT(Pattern);
  NODE_VISIT(WildcardPattern);
  NODE_VISIT(BoundPattern);
  NODE_VISIT(StarPattern);
  NODE_VISIT(IntPattern);
  NODE_VISIT(BoolPattern);
  NODE_VISIT(StrPattern);
  NODE_VISIT(SeqPattern);
  NODE_VISIT(RecordPattern);
  NODE_VISIT(ArrayPattern);
  NODE_VISIT(OptionalPattern);
  NODE_VISIT(RangePattern);
  NODE_VISIT(OrPattern);
  NODE_VISIT(GuardedPattern);

  DEFAULT_VISIT(Terminator);
  NODE_VISIT(JumpTerminator);
  NODE_VISIT(CondJumpTerminator);
  NODE_VISIT(ReturnTerminator);
  NODE_VISIT(YieldTerminator);
  NODE_VISIT(ThrowTerminator);
  NODE_VISIT(AssertTerminator);

  DEFAULT_VISIT(types::Type);
  NODE_VISIT(types::RecordType);
  NODE_VISIT(types::RefType);
  NODE_VISIT(types::FuncType);
  NODE_VISIT(types::PartialFuncType);
  NODE_VISIT(types::Optional);
  NODE_VISIT(types::Array);
  NODE_VISIT(types::Pointer);
  NODE_VISIT(types::Generator);
  NODE_VISIT(types::IntNType);
};

} // namespace codegen
} // namespace ir
} // namespace seq

#undef DEFAULT_VISIT
#undef NODE_VISIT
