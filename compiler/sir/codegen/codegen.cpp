#include "codegen.h"

#include "util/fmt/format.h"

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

namespace {

using namespace seq::ir;
using namespace llvm;

std::string getMagicSignature(const std::string &name,
                              const std::vector<std::shared_ptr<types::Type>> &types) {
  fmt::memory_buffer buf;
  for (auto it = types.begin(); it != types.end(); ++it) {
    fmt::format_to(buf, FMT_STRING("{}{}"), (*it)->getName(),
                   it + 1 == types.end() ? "" : ", ");
  }
  return fmt::format(FMT_STRING("{}[{}]"), name, std::string(buf.begin(), buf.end()));
}

} // namespace

namespace seq {
namespace ir {
namespace codegen {

using namespace llvm;

Value *CodegenVisitor::transform(std::shared_ptr<IRModule> module) { return nullptr; }
Value *CodegenVisitor::transform(std::shared_ptr<BasicBlock> block) { return nullptr; }
Value *CodegenVisitor::transform(std::shared_ptr<Var> var) { return nullptr; }
Value *CodegenVisitor::transform(std::shared_ptr<Instr> instr) { return nullptr; }
Value *CodegenVisitor::transform(std::shared_ptr<Rvalue> rval) { return nullptr; }
Value *CodegenVisitor::transform(std::shared_ptr<Lvalue> lval) { return nullptr; }
Value *CodegenVisitor::transform(std::shared_ptr<Operand> op) { return nullptr; }
Value *CodegenVisitor::transform(std::shared_ptr<Pattern> pat) { return nullptr; }
Value *CodegenVisitor::transform(std::shared_ptr<Terminator> term) { return nullptr; }
Type *CodegenVisitor::transform(std::shared_ptr<types::Type> typ) {
  auto lookedUpType = ctx->getLLVMType(typ);
  if (lookedUpType)
    return lookedUpType;

  CodegenVisitor v(ctx);
  typ->accept(v);
  return v.typeResult;
}

void CodegenVisitor::visit(std::shared_ptr<IRModule> node) {}

void CodegenVisitor::visit(std::shared_ptr<BasicBlock> node) {}

void CodegenVisitor::visit(std::shared_ptr<Func> node) {}
void CodegenVisitor::visit(std::shared_ptr<Var> node) {}

void CodegenVisitor::visit(std::shared_ptr<AssignInstr> node) {}
void CodegenVisitor::visit(std::shared_ptr<RvalueInstr> node) {}

void CodegenVisitor::visit(std::shared_ptr<MemberRvalue> node) {}
void CodegenVisitor::visit(std::shared_ptr<CallRvalue> node) {}
void CodegenVisitor::visit(std::shared_ptr<PartialCallRvalue> node) {}
void CodegenVisitor::visit(std::shared_ptr<OperandRvalue> node) {}
void CodegenVisitor::visit(std::shared_ptr<MatchRvalue> node) {}
void CodegenVisitor::visit(std::shared_ptr<PipelineRvalue> node) {}
void CodegenVisitor::visit(std::shared_ptr<StackAllocRvalue> node) {}

void CodegenVisitor::visit(std::shared_ptr<VarLvalue> node) {}
void CodegenVisitor::visit(std::shared_ptr<VarMemberLvalue> node) {}

void CodegenVisitor::visit(std::shared_ptr<VarOperand> node) {}
void CodegenVisitor::visit(std::shared_ptr<VarPointerOperand> node) {}
void CodegenVisitor::visit(std::shared_ptr<LiteralOperand> node) {}

void CodegenVisitor::visit(std::shared_ptr<WildcardPattern> node) {}
void CodegenVisitor::visit(std::shared_ptr<BoundPattern> node) {}
void CodegenVisitor::visit(std::shared_ptr<StarPattern> node) {}
void CodegenVisitor::visit(std::shared_ptr<IntPattern> node) {}
void CodegenVisitor::visit(std::shared_ptr<BoolPattern> node) {}
void CodegenVisitor::visit(std::shared_ptr<StrPattern> node) {}
void CodegenVisitor::visit(std::shared_ptr<SeqPattern> node) {}
void CodegenVisitor::visit(std::shared_ptr<RecordPattern> node) {}
void CodegenVisitor::visit(std::shared_ptr<ArrayPattern> node) {}
void CodegenVisitor::visit(std::shared_ptr<OptionalPattern> node) {}
void CodegenVisitor::visit(std::shared_ptr<RangePattern> node) {}
void CodegenVisitor::visit(std::shared_ptr<OrPattern> node) {}
void CodegenVisitor::visit(std::shared_ptr<GuardedPattern> node) {}

void CodegenVisitor::visit(std::shared_ptr<JumpTerminator> node) {}
void CodegenVisitor::visit(std::shared_ptr<CondJumpTerminator> node) {}
void CodegenVisitor::visit(std::shared_ptr<ReturnTerminator> node) {}
void CodegenVisitor::visit(std::shared_ptr<YieldTerminator> node) {}
void CodegenVisitor::visit(std::shared_ptr<ThrowTerminator> node) {}
void CodegenVisitor::visit(std::shared_ptr<AssertTerminator> node) {}

void CodegenVisitor::visit(std::shared_ptr<types::RecordType> memberedType) {
  auto *llvmType = StructType::get(ctx->getLLVMContext());

  std::vector<Type *> body;
  for (auto &bodyType : memberedType->getMemberTypes())
    body.push_back(bodyType->getId() == memberedType->getId() ? llvmType
                                                              : transform(bodyType));

  llvmType->setBody(body);
  llvmType->setName(memberedType->getName());

  auto dfltBuilder = [=](IRBuilder<> &builder) -> Value * {
    Value *self = UndefValue::get(llvmType);
    for (unsigned i = 0; i < memberedType->getMemberTypes().size(); i++) {
      auto *elem = ctx->getDefaultValue(memberedType->getMemberTypes()[i], builder);
      self = builder.CreateInsertValue(self, elem, i);
    }
    return self;
  };
  Context::InlineMagicFuncs inlineMagics = {
      {getMagicSignature("__new__", memberedType->getMemberTypes()),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         auto *val = dfltBuilder(builder);
         for (auto it = args.begin(); it != args.end(); ++it)
           val = builder.CreateInsertValue(val, *it, it - args.begin());
         return val;
       }},
  };
  Context::NonInlineMagicFuncs nonInlineMagicFuncs = {};
  // TODO  __iter__,  __contains__

  auto heterogeneous = memberedType->getMemberTypes().empty();
  if (!heterogeneous) {
    for (auto typ : memberedType->getMemberTypes()) {
      heterogeneous =
          heterogeneous || typ->getId() != memberedType->getMemberTypes()[0]->getId();
    }
  }

  if (!heterogeneous) {
    {
      auto llvmName = fmt::format(FMT_STRING("seq.{}{}.__getitem__"),
                                  memberedType->getName(), memberedType->getId());
      auto *module = ctx->getModule();
      auto &context = ctx->getLLVMContext();

      llvm::Type *baseType = body[0];
      auto *getitem = cast<Function>(module->getOrInsertFunction(
          llvmName, baseType, llvmType, seqIntLLVM(context)));
      getitem->setLinkage(GlobalValue::PrivateLinkage);

      auto iter = getitem->arg_begin();
      Value *self = iter++;
      Value *idx = iter;
      llvm::BasicBlock *entry = llvm::BasicBlock::Create(context, "entry", getitem);

      IRBuilder<> b(entry);
      b.SetInsertPoint(entry);
      Value *ptr = b.CreateAlloca(llvmType);
      b.CreateStore(self, ptr);
      ptr = b.CreateBitCast(ptr, baseType->getPointerTo());
      ptr = b.CreateGEP(ptr, idx);
      b.CreateRet(b.CreateLoad(ptr));

      auto name =
          fmt::format(FMT_STRING("__getitem__[{}, int]"), memberedType->getName());
      nonInlineMagicFuncs[name] = getitem;
    }
  }

  ctx->registerType(memberedType, llvmType, dfltBuilder, inlineMagics,
                    nonInlineMagicFuncs);
  typeResult = llvmType;
}

void CodegenVisitor::visit(std::shared_ptr<types::RefType> refType) {
  auto *llvmType = IntegerType::getInt8PtrTy(ctx->getLLVMContext());
  auto contents = refType->getContents();

  transform(contents);

  auto dfltBuilder = [=](IRBuilder<> &builder) -> Value * {
    return ConstantPointerNull::get(llvmType);
  };

  Context::InlineMagicFuncs inlineMagics = {
      {getMagicSignature("__new__", {}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         assert(refType->getContents());
         auto *self = alloc(contents, nullptr, builder);
         self = builder.CreateBitCast(self, llvmType);
         return self;
       }},
      {getMagicSignature("__raw__", {refType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return args[0];
       }},
  };
  ctx->registerType(refType, llvmType, dfltBuilder, inlineMagics, {});
  typeResult = llvmType;
}

void CodegenVisitor::visit(std::shared_ptr<types::FuncType> funcType) {
  auto *rType = transform(funcType->getRType());

  std::vector<Type *> args;
  for (auto argType : funcType->getArgTypes()) {
    args.push_back(transform(argType));
  }

  auto *llvmType = PointerType::get(FunctionType::get(rType, args, false), 0);

  auto dfltBuilder = [=](IRBuilder<> &builder) -> Value * {
    return ConstantPointerNull::get(llvmType);
  };

  // TODO: confirm magics not necessary for functions
  ctx->registerType(funcType, llvmType, dfltBuilder, {}, {});
  typeResult = llvmType;
}

void CodegenVisitor::visit(std::shared_ptr<types::PartialFuncType> partialFuncType) {
  auto *llvmType = StructType::get(ctx->getLLVMContext());

  std::vector<Type *> body;
  for (auto &bodyType : partialFuncType->getCallTypes())
    if (bodyType)
      body.push_back(transform(bodyType));

  llvmType->setBody(body);
  llvmType->setName(partialFuncType->getName());

  auto dfltBuilder = [=](IRBuilder<> &builder) -> Value * {
    Value *self = UndefValue::get(llvmType);
    for (unsigned i = 0; i < partialFuncType->getCallTypes().size(); i++) {
      if (partialFuncType->getCallTypes()[i]) {
        auto *elem = ctx->getDefaultValue(partialFuncType->getCallTypes()[i], builder);
        self = builder.CreateInsertValue(self, elem, i);
      }
    }
    return self;
  };

  // TODO: confirm magics not necessary for partial functions
  ctx->registerType(partialFuncType, llvmType, dfltBuilder, {}, {});
  typeResult = llvmType;
}

void CodegenVisitor::visit(std::shared_ptr<types::Optional> optType) {
  auto *baseLLVMType = transform(optType->getBase());
  auto *llvmType = optType->getBase()->isRef()
                       ? baseLLVMType
                       : StructType::get(IntegerType::getInt1Ty(ctx->getLLVMContext()),
                                         baseLLVMType);
  auto dfltBuilder = [=](IRBuilder<> &builder) -> Value * {
    if (optType->getBase()->isRef())
      return ConstantPointerNull::get(cast<PointerType>(llvmType));
    else {
      Value *self = UndefValue::get(llvmType);
      self = builder.CreateInsertValue(
          self, ConstantInt::get(IntegerType::getInt1Ty(ctx->getLLVMContext()), 0), 0);
      return self;
    }
  };

  Context::InlineMagicFuncs inlineMagics = {
      {getMagicSignature("__new__", {optType->getBase()}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         if (optType->getBase()->isRef())
           return args[0];
         else {
           Value *self = UndefValue::get(llvmType);
           self = builder.CreateInsertValue(
               self, ConstantInt::get(IntegerType::getInt1Ty(ctx->getLLVMContext()), 1),
               0);
           self = builder.CreateInsertValue(self, args[0], 1);
           return self;
         }
       }},
      {getMagicSignature("__new__", {}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return dfltBuilder(builder);
       }},
      {getMagicSignature("__bool__", {optType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         if (optType->getBase()->isRef()) {
           return builder.CreateICmpNE(
               args[0], ConstantPointerNull::get(cast<PointerType>(llvmType)));
         } else {
           return builder.CreateZExt(builder.CreateExtractValue(args[0], 1),
                                     ctx->getLLVMType(types::kBoolType));
         }
       }},
      {getMagicSignature("__invert__", {optType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return optType->getBase()->isRef() ? args[0]
                                            : builder.CreateExtractValue(args[0], 1);
       }},
  };

  ctx->registerType(optType, llvmType, dfltBuilder, inlineMagics, {});
  typeResult = llvmType;
}

void CodegenVisitor::visit(std::shared_ptr<types::Array> arrayType) {
  auto baseType = arrayType->getBase();
  auto *baseLLVMType = transform(baseType);

  auto *llvmType = StructType::get(seqIntLLVM(ctx->getLLVMContext()),
                                   PointerType::get(baseLLVMType, 0));

  auto dfltBuilder = [=](IRBuilder<> &builder) -> Value * {
    auto &context = ctx->getLLVMContext();
    Value *self = UndefValue::get(llvmType);
    self = builder.CreateInsertValue(self, zeroLLVM(context), 0);
    self = builder.CreateInsertValue(
        self, ConstantPointerNull::get(PointerType::get(baseLLVMType, 0)), 1); // gubus
    return self;
  };

  Context::InlineMagicFuncs inlineMagics = {
      {getMagicSignature("__new__", {types::kIntType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         auto *ptr = alloc(baseType, args[0], builder);
         Value *self = UndefValue::get(llvmType);
         self = builder.CreateInsertValue(self, args[0], 0);
         self = builder.CreateInsertValue(self, ptr, 1);
         return self;
       }},
      {fmt::format(FMT_STRING("__new__[Pointer[{}], int]"), baseType->getName()),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         Value *self = UndefValue::get(llvmType);
         self = builder.CreateInsertValue(self, args[0], 0);
         self = builder.CreateInsertValue(self, args[1], 1);
         return self;
       }},
      {getMagicSignature("__copy__", {arrayType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         Value *len = builder.CreateExtractValue(args[0], 0);
         Value *otherPtr = builder.CreateExtractValue(args[0], 1);

         auto *ptr = alloc(baseType, len, builder);

         Value *self = UndefValue::get(llvmType);
         self = builder.CreateInsertValue(self, len, 0);
         self = builder.CreateInsertValue(self, ptr, 1);

         auto *bytes = builder.CreateMul(len, ConstantExpr::getSizeOf(baseLLVMType));
         makeMemCpy(ptr, otherPtr, bytes, builder.GetInsertBlock());

         return self;
       }},
      {getMagicSignature("__len__", {arrayType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateExtractValue(args[0], 0);
       }},
      {getMagicSignature("__bool__", {arrayType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         Value *len = builder.CreateExtractValue(args[0], 0);
         Value *zero = ConstantInt::get(seqIntLLVM(ctx->getLLVMContext()), 0);
         return builder.CreateZExt(builder.CreateICmpNE(len, zero),
                                   ctx->getLLVMType(types::kBoolType));
       }},
      {getMagicSignature("__getitem__", {arrayType, types::kIntType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         Value *ptr = builder.CreateExtractValue(args[0], 1);
         ptr = builder.CreateGEP(ptr, args[1]);
         return builder.CreateLoad(ptr);
       }},
      {getMagicSignature("__slice__", {arrayType, types::kIntType, types::kIntType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         Value *ptr = builder.CreateExtractValue(args[0], 1);
         ptr = builder.CreateGEP(ptr, args[1]);

         Value *len = builder.CreateSub(args[2], args[1]);

         Value *slice = UndefValue::get(llvmType);

         slice = builder.CreateInsertValue(slice, len, 0);
         slice = builder.CreateInsertValue(slice, ptr, 1);
         return slice;
       }},
      {getMagicSignature("__slice_left__", {arrayType, types::kIntType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         Value *ptr = builder.CreateExtractValue(args[0], 1);

         Value *slice = UndefValue::get(llvmType);

         slice = builder.CreateInsertValue(slice, args[1], 0);
         slice = builder.CreateInsertValue(slice, ptr, 1);
         return slice;
       }},
      {getMagicSignature("__slice_right__", {arrayType, types::kIntType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         Value *ptr = builder.CreateExtractValue(args[0], 1);
         ptr = builder.CreateGEP(ptr, args[1]);

         Value *to = builder.CreateExtractValue(args[0], 1);
         Value *len = builder.CreateSub(to, args[1]);

         Value *slice = UndefValue::get(llvmType);

         slice = builder.CreateInsertValue(slice, len, 0);
         slice = builder.CreateInsertValue(slice, ptr, 1);
         return slice;
       }},
      {getMagicSignature("__setitem__", {arrayType, types::kIntType, baseType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         Value *ptr = builder.CreateExtractValue(args[0], 1);
         ptr = builder.CreateGEP(ptr, args[1]);
         builder.CreateStore(args[2], ptr);
         return (Value *)nullptr;
       }},
  };
  ctx->registerType(arrayType, llvmType, dfltBuilder, inlineMagics, {});
  typeResult = llvmType;
}

void CodegenVisitor::visit(std::shared_ptr<types::Pointer> pointerType) {
  auto baseType = pointerType->getBase();
  auto *baseLLVMType = transform(baseType);

  auto *llvmType = PointerType::get(baseLLVMType, 0);

  auto dfltBuilder = [=](IRBuilder<> &builder) -> Value * {
    return ConstantPointerNull::get(llvmType);
  };

  Context::InlineMagicFuncs inlineMagics = {
      {getMagicSignature("__elemsize__", {}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return ConstantExpr::getSizeOf(baseLLVMType);
       }},
      {getMagicSignature("__atomic__", {}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return ConstantInt::get(ctx->getLLVMType(types::kBoolType),
                                 baseType->isAtomic() ? 1 : 0);
       }},
      {getMagicSignature("__new__", {}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return dfltBuilder(builder);
       }},
      {getMagicSignature("__new__", {types::kIntType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return alloc(baseType, args[0], builder);
       }},
      {"__new__[Pointer[byte]]",
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateBitCast(args[0], llvmType);
       }},
      {getMagicSignature("as_byte", {pointerType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateBitCast(args[0],
                                      IntegerType::getInt8PtrTy(ctx->getLLVMContext()));
       }},
      {getMagicSignature("__int__", {pointerType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreatePtrToInt(args[0], seqIntLLVM(ctx->getLLVMContext()));
       }},
      {getMagicSignature("__copy__", {pointerType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return args[0];
       }},
      {getMagicSignature("__bool__", {pointerType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateZExt(builder.CreateIsNotNull(args[0]),
                                   ctx->getLLVMType(types::kBoolType));
       }},
      {getMagicSignature("__getitem__", {pointerType, types::kIntType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         Value *ptr = builder.CreateGEP(args[0], args[1]);
         return builder.CreateLoad(ptr);
       }},
      {getMagicSignature("__setitem__", {pointerType, types::kIntType, baseType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         Value *ptr = builder.CreateGEP(args[0], args[1]);
         builder.CreateStore(args[2], ptr);
         return (Value *)nullptr;
       }},
      {getMagicSignature("__add__", {pointerType, types::kIntType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateGEP(args[0], args[1]);
       }},
      {getMagicSignature("__sub__", {pointerType, pointerType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreatePtrDiff(args[0], args[1]);
       }},
      {getMagicSignature("__eq__", {pointerType, pointerType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateZExt(builder.CreateICmpEQ(args[0], args[1]),
                                   ctx->getLLVMType(types::kBoolType));
       }},
      {getMagicSignature("__ne__", {pointerType, pointerType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateZExt(builder.CreateICmpNE(args[0], args[1]),
                                   ctx->getLLVMType(types::kBoolType));
       }},
      {getMagicSignature("__lt__", {pointerType, pointerType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateZExt(builder.CreateICmpSLT(args[0], args[1]),
                                   ctx->getLLVMType(types::kBoolType));
       }},
      {getMagicSignature("__gt__", {pointerType, pointerType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateZExt(builder.CreateICmpSGT(args[0], args[1]),
                                   ctx->getLLVMType(types::kBoolType));
       }},
      {getMagicSignature("__le__", {pointerType, pointerType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateZExt(builder.CreateICmpSLE(args[0], args[1]),
                                   ctx->getLLVMType(types::kBoolType));
       }},
      {getMagicSignature("__ge__", {pointerType, pointerType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateZExt(builder.CreateICmpSGE(args[0], args[1]),
                                   ctx->getLLVMType(types::kBoolType));
       }},
      // prefetch // TODO
      {getMagicSignature("__setitem__", {arrayType, types::kIntType, baseType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return nullptr;
       }},
      {getMagicSignature("__setitem__", {arrayType, types::kIntType, baseType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return nullptr;
       }},
      {getMagicSignature("__setitem__", {arrayType, types::kIntType, baseType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return nullptr;
       }},
      {getMagicSignature("__setitem__", {arrayType, types::kIntType, baseType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return nullptr;
       }},
      {getMagicSignature("__setitem__", {arrayType, types::kIntType, baseType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return nullptr;
       }},
      {getMagicSignature("__setitem__", {arrayType, types::kIntType, baseType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return nullptr;
       }},
      {getMagicSignature("__setitem__", {arrayType, types::kIntType, baseType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return nullptr;
       }},
  };
}

void CodegenVisitor::visit(std::shared_ptr<types::Generator> node) {}
void CodegenVisitor::visit(std::shared_ptr<types::IntNType> node) {}

Value *CodegenVisitor::callMagic(std::shared_ptr<types::Type> type,
                                 const std::string &sig, std::vector<Value *> args,
                                 IRBuilder<> &builder) {
  // TODO
  return nullptr;
}

Value *CodegenVisitor::callBuiltin(const std::string &sig, std::vector<Value *> args,
                                   IRBuilder<> &builder) {
  // TODO
  return nullptr;
}

Value *CodegenVisitor::alloc(std::shared_ptr<types::Type> type, Value *count,
                             IRBuilder<> &builder) {
  // TODO
  return nullptr;
}

} // namespace codegen
} // namespace ir
} // namespace seq