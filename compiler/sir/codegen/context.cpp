#include "context.h"

#include "sir/bblock.h"
#include "sir/var.h"
#include "sir/types/types.h"

#include "util/common.h"
#include "util/fmt/format.h"

#include "common.h"

namespace seq {
namespace ir {
namespace codegen {

using namespace llvm;

void Context::registerType(std::shared_ptr<types::Type> sirType, llvm::Type *llvmType,
                           DefaultValueBuilder dfltBuilder,
                           InlineMagicFuncs inlineMagics,
                           NonInlineMagicFuncs nonInlineMagics) {
  typeRealizations[sirType->getId()] = llvmType;
  defaultValueBuilders[sirType->getId()] = std::move(dfltBuilder);
  inlineMagicBuilders[sirType->getId()] = std::move(inlineMagics);
  nonInlineMagicFuncs[sirType->getId()] = std::move(nonInlineMagics);
}

void Context::registerVar(std::shared_ptr<Var> sirVar, llvm::Value *val) {
  auto &frame = getFrame();
  frame.varRealizations[sirVar->getId()] = val;
}

void Context::registerBlock(std::shared_ptr<BasicBlock> sirBlock,
                            llvm::BasicBlock *block) {
  auto &frame = getFrame();
  frame.blockRealizations[sirBlock->getId()] = block;
}

llvm::Value *Context::getVar(std::shared_ptr<Var> sirVar) {
  auto &frame = getFrame();
  auto it = frame.varRealizations.find(sirVar->getId());
  return it == frame.varRealizations.end() ? nullptr : it->second;
}

llvm::BasicBlock *Context::getBlock(std::shared_ptr<BasicBlock> sirBlock) {
  auto &frame = getFrame();
  auto it = frame.blockRealizations.find(sirBlock->getId());
  return it == frame.blockRealizations.end() ? nullptr : it->second;
}

llvm::Type *Context::getLLVMType(std::shared_ptr<types::Type> type) {
  auto it = typeRealizations.find(type->getId());
  return it == typeRealizations.end() ? nullptr : it->second;
}

llvm::Value *Context::getDefaultValue(std::shared_ptr<types::Type> type,
                                      llvm::IRBuilder<> &builder) {
  auto it = defaultValueBuilders.find(type->getId());
  return it == defaultValueBuilders.end() ? nullptr : it->second(builder);
}

void Context::initTypeRealizations() {
  auto &llvmCtx = getLLVMContext();

  typeRealizations[types::kStringType->getId()] =
      StructType::get(seqIntLLVM(llvmCtx), IntegerType::getInt8PtrTy(llvmCtx));
  defaultValueBuilders[types::kStringType->getId()] =
      [this](IRBuilder<> &builder) -> Value * {
    Value *self = UndefValue::get(typeRealizations[types::kStringType->getId()]);
    self = builder.CreateInsertValue(
        self, ConstantInt::get(seqIntLLVM(getLLVMContext()), 0), 0);
    self = builder.CreateInsertValue(
        self, ConstantPointerNull::get(IntegerType::getInt8PtrTy(getLLVMContext())), 1);
    return self;
  };

  typeRealizations[types::kBoolType->getId()] = IntegerType::getInt8Ty(llvmCtx);
  defaultValueBuilders[types::kBoolType->getId()] =
      [this](IRBuilder<> &builder) -> Value * {
    return ConstantInt::get(typeRealizations[types::kBoolType->getId()], 0);
  };

  typeRealizations[types::kFloatType->getId()] = Type::getDoubleTy(llvmCtx);
  defaultValueBuilders[types::kFloatType->getId()] =
      [this](IRBuilder<> &builder) -> Value * {
    return ConstantFP::get(typeRealizations[types::kFloatType->getId()], 0.0);
  };

  typeRealizations[types::kIntType->getId()] = seqIntLLVM(llvmCtx);
  defaultValueBuilders[types::kIntType->getId()] =
      [this](IRBuilder<> &builder) -> Value * {
    return ConstantInt::get(typeRealizations[types::kIntType->getId()], 0);
  };

  typeRealizations[types::kByteType->getId()] = IntegerType::getInt8Ty(llvmCtx);
  defaultValueBuilders[types::kByteType->getId()] =
      [this](IRBuilder<> &builder) -> Value * {
    return ConstantInt::get(typeRealizations[types::kByteType->getId()], 0);
  };

  typeRealizations[types::kByteType->getId()] = IntegerType::getInt8Ty(llvmCtx);
  defaultValueBuilders[types::kByteType->getId()] =
      [this](IRBuilder<> &builder) -> Value * {
    return ConstantInt::get(typeRealizations[types::kByteType->getId()], 0);
  };

  typeRealizations[types::kSeqType->getId()] = nullptr;
  typeRealizations[types::kAnyType->getId()] = nullptr;
  typeRealizations[types::kVoidType->getId()] = nullptr;
}

void Context::initMagicFuncs() {
  // TODO seq/kmer!
  inlineMagicBuilders = {
      {types::kStringType->getId(),
       {
           {"__new__[Pointer[byte], int]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *type = typeRealizations[types::kStringType->getId()];
              auto *self = UndefValue::get(type);
              builder.CreateInsertValue(self, args[0], 0);
              builder.CreateInsertValue(self, args[1], 1);
              return self;
            }},
       }},
      {types::kBoolType->getId(),
       {
           {"__new__[]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              return defaultValueBuilders[types::kBoolType->getId()](builder);
            }},
           {"__str__[bool]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *self = args[0];
              auto *type = typeRealizations[types::kBoolType->getId()];
              auto *strType = typeRealizations[types::kStringType->getId()];
              auto *strFunc = cast<Function>(
                  module->getOrInsertFunction("seq_str_bool", strType, type));
              strFunc->setDoesNotThrow();
              return builder.CreateCall(strFunc, self);
            }},
           {"__copy__[bool]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              return args[0];
            }},
           {"__bool__[bool]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              return args[0];
            }},
           {"__invert__[bool]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *type = typeRealizations[types::kBoolType->getId()];
              return builder.CreateZExt(
                  builder.CreateNot(builder.CreateTrunc(
                      args[0], IntegerType::getInt1Ty(builder.getContext()))),
                  type);
            }},
           {"__eq__[bool, bool]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *type = typeRealizations[types::kBoolType->getId()];
              return builder.CreateZExt(builder.CreateICmpEQ(args[0], args[1]), type);
            }},
           {"__ne__[bool, bool]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *type = typeRealizations[types::kBoolType->getId()];
              return builder.CreateZExt(builder.CreateICmpNE(args[0], args[1]), type);
            }},
           {"__lt__[bool, bool]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *type = typeRealizations[types::kBoolType->getId()];
              return builder.CreateZExt(builder.CreateICmpULT(args[0], args[1]), type);
            }},
           {"__gt__[bool, bool]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *type = typeRealizations[types::kBoolType->getId()];
              return builder.CreateZExt(builder.CreateICmpUGT(args[0], args[1]), type);
            }},
           {"__le__[bool, bool]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *type = typeRealizations[types::kBoolType->getId()];
              return builder.CreateZExt(builder.CreateICmpULE(args[0], args[1]), type);
            }},
           {"__ge__[bool, bool]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *type = typeRealizations[types::kBoolType->getId()];
              return builder.CreateZExt(builder.CreateICmpUGE(args[0], args[1]), type);
            }},
           {"__xor__[bool, bool]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              return builder.CreateXor(args[0], args[1]);
            }},
       }},
      {types::kFloatType->getId(),
       {
           {"__new__[]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              return defaultValueBuilders[types::kFloatType->getId()](builder);
            }},
           {"__new__[float]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              return args[0];
            }},
           {"__new__[int]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              return builder.CreateSIToFP(args[0],
                                          typeRealizations[types::kFloatType->getId()]);
            }},
           {"__str__[float]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *self = args[0];
              auto *type = typeRealizations[types::kFloatType->getId()];
              auto *strType = typeRealizations[types::kStringType->getId()];
              auto *strFunc = cast<Function>(
                  module->getOrInsertFunction("seq_str_float", strType, type));
              strFunc->setDoesNotThrow();
              return builder.CreateCall(strFunc, self);
            }},
           {"__copy__[float]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              return args[0];
            }},
           {"__bool__[float]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              return builder.CreateZExt(
                  builder.CreateFCmpONE(
                      args[0], ConstantFP::get(
                                   typeRealizations[types::kFloatType->getId()], 0.0)),
                  typeRealizations[types::kBoolType->getId()]);
            }},
           {"__pos__[float]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              return args[0];
            }},
           {"__neg__[float]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              return builder.CreateFNeg(args[0]);
            }},
           // float-float binary
           {"__add__[float, float]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              return builder.CreateFAdd(args[0], args[1]);
            }},
           {"__sub__[float, float]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              return builder.CreateFSub(args[0], args[1]);
            }},
           {"__mul__[float, float]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              return builder.CreateFMul(args[0], args[1]);
            }},
           {"__div__[float, float]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *type = typeRealizations[types::kFloatType->getId()];
              Value *v = builder.CreateFDiv(args[0], args[1]);
              Function *floor = Intrinsic::getDeclaration(
                  builder.GetInsertBlock()->getModule(), Intrinsic::floor, {type});
              return builder.CreateCall(floor, v);
            }},
           {"__truediv__[float, float]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              return builder.CreateFDiv(args[0], args[1]);
            }},
           {"__mod__[float, float]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              return builder.CreateFRem(args[0], args[1]);
            }},
           {"__pow__[float, float]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *type = typeRealizations[types::kFloatType->getId()];
              Function *pow = Intrinsic::getDeclaration(
                  builder.GetInsertBlock()->getModule(), Intrinsic::pow, {type});
              return builder.CreateCall(pow, {args[0], args[1]});
            }},
           {"__eq__[float, float]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *boolType = typeRealizations[types::kBoolType->getId()];
              return builder.CreateZExt(builder.CreateFCmpOEQ(args[0], args[1]),
                                        boolType);
            }},
           {"__ne__[float, float]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *boolType = typeRealizations[types::kBoolType->getId()];
              return builder.CreateZExt(builder.CreateFCmpONE(args[0], args[1]),
                                        boolType);
            }},
           {"__lt__[float, float]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *boolType = typeRealizations[types::kBoolType->getId()];
              return builder.CreateZExt(builder.CreateFCmpOLT(args[0], args[1]),
                                        boolType);
            }},
           {"__gt__[float, float]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *boolType = typeRealizations[types::kBoolType->getId()];
              return builder.CreateZExt(builder.CreateFCmpOGT(args[0], args[1]),
                                        boolType);
            }},
           {"__le__[float, float]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *boolType = typeRealizations[types::kBoolType->getId()];
              return builder.CreateZExt(builder.CreateFCmpOLE(args[0], args[1]),
                                        boolType);
            }},
           {"__ge__[float, float]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *boolType = typeRealizations[types::kBoolType->getId()];
              return builder.CreateZExt(builder.CreateFCmpOGE(args[0], args[1]),
                                        boolType);
            }},
           // float-int
           {"__add__[float, int]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *floatType = typeRealizations[types::kFloatType->getId()];
              args[1] = builder.CreateSIToFP(args[1], floatType);
              return builder.CreateFAdd(args[0], args[1]);
            }},
           {"__sub__[float, int]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *floatType = typeRealizations[types::kFloatType->getId()];
              args[1] = builder.CreateSIToFP(args[1], floatType);
              return builder.CreateFSub(args[0], args[1]);
            }},
           {"__mul__[float, int]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *floatType = typeRealizations[types::kFloatType->getId()];
              args[1] = builder.CreateSIToFP(args[1], floatType);
              return builder.CreateFMul(args[0], args[1]);
            }},
           {"__div__[float, int]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *type = typeRealizations[types::kFloatType->getId()];
              args[1] = builder.CreateSIToFP(args[1], type);
              Value *v = builder.CreateFDiv(args[0], args[1]);
              Function *floor = Intrinsic::getDeclaration(
                  builder.GetInsertBlock()->getModule(), Intrinsic::floor, {type});
              return builder.CreateCall(floor, v);
            }},
           {"__truediv__[float, int]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *floatType = typeRealizations[types::kFloatType->getId()];
              args[1] = builder.CreateSIToFP(args[1], floatType);
              return builder.CreateFDiv(args[0], args[1]);
            }},
           {"__mod__[float, int]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *floatType = typeRealizations[types::kFloatType->getId()];
              args[1] = builder.CreateSIToFP(args[1], floatType);
              return builder.CreateFRem(args[0], args[1]);
            }},
           {"__pow__[float, int]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *type = typeRealizations[types::kFloatType->getId()];
              args[1] = builder.CreateSIToFP(args[1], type);
              Function *pow = Intrinsic::getDeclaration(
                  builder.GetInsertBlock()->getModule(), Intrinsic::pow, {type});
              return builder.CreateCall(pow, {args[0], args[1]});
            }},
           {"__eq__[float, int]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *floatType = typeRealizations[types::kFloatType->getId()];
              args[1] = builder.CreateSIToFP(args[1], floatType);
              auto *boolType = typeRealizations[types::kBoolType->getId()];
              return builder.CreateZExt(builder.CreateFCmpOEQ(args[0], args[1]),
                                        boolType);
            }},
           {"__ne__[float, int]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *boolType = typeRealizations[types::kBoolType->getId()];
              return builder.CreateZExt(builder.CreateFCmpONE(args[0], args[1]),
                                        boolType);
            }},
           {"__lt__[float, int]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *floatType = typeRealizations[types::kFloatType->getId()];
              args[1] = builder.CreateSIToFP(args[1], floatType);
              auto *boolType = typeRealizations[types::kBoolType->getId()];
              return builder.CreateZExt(builder.CreateFCmpOLT(args[0], args[1]),
                                        boolType);
            }},
           {"__gt__[float, int]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *floatType = typeRealizations[types::kFloatType->getId()];
              args[1] = builder.CreateSIToFP(args[1], floatType);
              auto *boolType = typeRealizations[types::kBoolType->getId()];
              return builder.CreateZExt(builder.CreateFCmpOGT(args[0], args[1]),
                                        boolType);
            }},
           {"__le__[float, int]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *floatType = typeRealizations[types::kFloatType->getId()];
              args[1] = builder.CreateSIToFP(args[1], floatType);
              auto *boolType = typeRealizations[types::kBoolType->getId()];
              return builder.CreateZExt(builder.CreateFCmpOLE(args[0], args[1]),
                                        boolType);
            }},
           {"__ge__[float, int]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *floatType = typeRealizations[types::kFloatType->getId()];
              args[1] = builder.CreateSIToFP(args[1], floatType);
              auto *boolType = typeRealizations[types::kBoolType->getId()];
              return builder.CreateZExt(builder.CreateFCmpOGE(args[0], args[1]),
                                        boolType);
            }},
       }},
      {types::kIntType->getId(),
       {
           {"__new__[]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              return defaultValueBuilders[types::kIntType->getId()](builder);
            }},
           {"__new__[int]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              return args[0];
            }},
           {"__new__[float]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              return builder.CreateFPToSI(args[0],
                                          typeRealizations[types::kIntType->getId()]);
            }},
           {"__new__[bool]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              return builder.CreateZExt(args[0],
                                        typeRealizations[types::kIntType->getId()]);
            }},
           {"__new__[byte]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              return builder.CreateZExt(args[0],
                                        typeRealizations[types::kIntType->getId()]);
            }},
           {"__str__[int]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *self = args[0];
              auto *type = typeRealizations[types::kIntType->getId()];
              auto *strType = typeRealizations[types::kStringType->getId()];
              auto *strFunc = cast<Function>(
                  module->getOrInsertFunction("seq_str_int", strType, type));
              strFunc->setDoesNotThrow();
              return builder.CreateCall(strFunc, self);
            }},
           {"__copy__[int]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              return args[0];
            }},
           {"__hash__[int]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              return args[0];
            }},
           {"__bool__[int]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              return builder.CreateZExt(
                  builder.CreateICmpEQ(
                      args[0],
                      ConstantInt::get(typeRealizations[types::kIntType->getId()], 0)),
                  typeRealizations[types::kBoolType->getId()]);
            }},
           {"__pos__[int]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              return args[0];
            }},
           {"__neg__[int]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              return builder.CreateNeg(args[0]);
            }},
           // int-int binary
           {"__add__[int, int]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              return builder.CreateAdd(args[0], args[1]);
            }},
           {"__sub__[int, int]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              return builder.CreateSub(args[0], args[1]);
            }},
           {"__mul__[int, int]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              return builder.CreateMul(args[0], args[1]);
            }},
           {"__truediv__[int, int]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *floatType = typeRealizations[types::kFloatType->getId()];
              args[0] = builder.CreateSIToFP(args[0], floatType);
              args[1] = builder.CreateSIToFP(args[1], floatType);
              return builder.CreateFDiv(args[0], args[1]);
            }},
           {"__div__[int, int]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              return builder.CreateSDiv(args[0], args[1]);
            }},
           {"__mod__[int, int]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              return builder.CreateSRem(args[0], args[1]);
            }},
           {"__lshift__[int, int]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              return builder.CreateShl(args[0], args[1]);
            }},
           {"__rshift__[int, int]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              return builder.CreateAShr(args[0], args[1]);
            }},
           {"__eq__[int, int]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *boolType = typeRealizations[types::kBoolType->getId()];
              return builder.CreateZExt(builder.CreateICmpEQ(args[0], args[1]),
                                        boolType);
            }},
           {"__ne__[int, int]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *boolType = typeRealizations[types::kBoolType->getId()];
              return builder.CreateZExt(builder.CreateICmpNE(args[0], args[1]),
                                        boolType);
            }},
           {"__lt__[int, int]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *boolType = typeRealizations[types::kBoolType->getId()];
              return builder.CreateZExt(builder.CreateICmpSLT(args[0], args[1]),
                                        boolType);
            }},
           {"__gt__[int, int]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *boolType = typeRealizations[types::kBoolType->getId()];
              return builder.CreateZExt(builder.CreateICmpSGT(args[0], args[1]),
                                        boolType);
            }},
           {"__le__[int, int]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *boolType = typeRealizations[types::kBoolType->getId()];
              return builder.CreateZExt(builder.CreateICmpSLE(args[0], args[1]),
                                        boolType);
            }},
           {"__ge__[int, int]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *boolType = typeRealizations[types::kBoolType->getId()];
              return builder.CreateZExt(builder.CreateICmpSLE(args[0], args[1]),
                                        boolType);
            }},
           // int-float
           {"__add__[int, float]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *floatType = typeRealizations[types::kFloatType->getId()];
              args[0] = builder.CreateSIToFP(args[0], floatType);
              return builder.CreateFAdd(args[0], args[1]);
            }},
           {"__sub__[int, float]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *floatType = typeRealizations[types::kFloatType->getId()];
              args[0] = builder.CreateSIToFP(args[0], floatType);
              return builder.CreateFSub(args[0], args[1]);
            }},
           {"__mul__[int, float]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *floatType = typeRealizations[types::kFloatType->getId()];
              args[0] = builder.CreateSIToFP(args[0], floatType);
              return builder.CreateFMul(args[0], args[1]);
            }},
           {"__div__[int, float]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *type = typeRealizations[types::kFloatType->getId()];
              args[1] = builder.CreateSIToFP(args[1], type);
              Value *v = builder.CreateFDiv(args[0], args[1]);
              Function *floor = Intrinsic::getDeclaration(
                  builder.GetInsertBlock()->getModule(), Intrinsic::floor, {type});
              return builder.CreateCall(floor, v);
            }},
           {"__truediv__[int, float]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *floatType = typeRealizations[types::kFloatType->getId()];
              args[0] = builder.CreateSIToFP(args[0], floatType);
              return builder.CreateFDiv(args[0], args[1]);
            }},
           {"__mod__[int, float]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *floatType = typeRealizations[types::kFloatType->getId()];
              args[0] = builder.CreateSIToFP(args[0], floatType);
              return builder.CreateFRem(args[0], args[1]);
            }},
           {"__pow__[int, float]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *type = typeRealizations[types::kFloatType->getId()];
              args[1] = builder.CreateSIToFP(args[1], type);
              Function *pow = Intrinsic::getDeclaration(
                  builder.GetInsertBlock()->getModule(), Intrinsic::pow, {type});
              return builder.CreateCall(pow, {args[0], args[1]});
            }},
           {"__eq__[int, float]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *floatType = typeRealizations[types::kFloatType->getId()];
              args[0] = builder.CreateSIToFP(args[0], floatType);
              auto *boolType = typeRealizations[types::kBoolType->getId()];
              return builder.CreateZExt(builder.CreateFCmpOEQ(args[0], args[1]),
                                        boolType);
            }},
           {"__ne__[int, float]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *boolType = typeRealizations[types::kBoolType->getId()];
              return builder.CreateZExt(builder.CreateFCmpONE(args[0], args[1]),
                                        boolType);
            }},
           {"__lt__[int, float]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *floatType = typeRealizations[types::kFloatType->getId()];
              args[0] = builder.CreateSIToFP(args[0], floatType);
              auto *boolType = typeRealizations[types::kBoolType->getId()];
              return builder.CreateZExt(builder.CreateFCmpOLT(args[0], args[1]),
                                        boolType);
            }},
           {"__gt__[int, float]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *floatType = typeRealizations[types::kFloatType->getId()];
              args[0] = builder.CreateSIToFP(args[0], floatType);
              auto *boolType = typeRealizations[types::kBoolType->getId()];
              return builder.CreateZExt(builder.CreateFCmpOGT(args[0], args[1]),
                                        boolType);
            }},
           {"__le__[int, float]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *floatType = typeRealizations[types::kFloatType->getId()];
              args[0] = builder.CreateSIToFP(args[0], floatType);
              auto *boolType = typeRealizations[types::kBoolType->getId()];
              return builder.CreateZExt(builder.CreateFCmpOLE(args[0], args[1]),
                                        boolType);
            }},
           {"__ge__[int, float]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *floatType = typeRealizations[types::kFloatType->getId()];
              args[0] = builder.CreateSIToFP(args[0], floatType);
              auto *boolType = typeRealizations[types::kBoolType->getId()];
              return builder.CreateZExt(builder.CreateFCmpOGE(args[0], args[1]),
                                        boolType);
            }},
       }},
      {types::kByteType->getId(),
       {
           {"__new__[]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              return defaultValueBuilders[types::kByteType->getId()](builder);
            }},
           {"__new__[byte]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              return args[0];
            }},
           {"__new__[int]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              return builder.CreateTrunc(args[0],
                                         typeRealizations[types::kByteType->getId()]);
            }},
           {"__str__[byte]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *self = args[0];
              auto *type = typeRealizations[types::kByteType->getId()];
              auto *strType = typeRealizations[types::kStringType->getId()];
              auto *strFunc = cast<Function>(
                  module->getOrInsertFunction("seq_str_byte", strType, type));
              strFunc->setDoesNotThrow();
              return builder.CreateCall(strFunc, self);
            }},
           {"__copy__[byte]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              return args[0];
            }},
           {"__bool__[byte]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *boolType = typeRealizations[types::kBoolType->getId()];
              auto *type = typeRealizations[types::kByteType->getId()];
              Value *zero = ConstantInt::get(type, 0);
              return builder.CreateZExt(builder.CreateFCmpONE(args[0], zero), boolType);
            }},
           {"__eq__[byte, byte]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *type = typeRealizations[types::kBoolType->getId()];
              return builder.CreateZExt(builder.CreateICmpEQ(args[0], args[1]), type);
            }},
           {"__ne__[byte, byte]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *type = typeRealizations[types::kBoolType->getId()];
              return builder.CreateZExt(builder.CreateICmpNE(args[0], args[1]), type);
            }},
           {"__lt__[byte, byte]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *type = typeRealizations[types::kBoolType->getId()];
              return builder.CreateZExt(builder.CreateICmpULT(args[0], args[1]), type);
            }},
           {"__gt__[byte, byte]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *type = typeRealizations[types::kBoolType->getId()];
              return builder.CreateZExt(builder.CreateICmpUGT(args[0], args[1]), type);
            }},
           {"__le__[byte, byte]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *type = typeRealizations[types::kBoolType->getId()];
              return builder.CreateZExt(builder.CreateICmpULE(args[0], args[1]), type);
            }},
           {"__ge__[byte, byte]",
            [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
              auto *type = typeRealizations[types::kBoolType->getId()];
              return builder.CreateZExt(builder.CreateICmpUGE(args[0], args[1]), type);
            }},
       }},
  };
  for (unsigned i = 1; i <= types::IntNType::MAX_LEN; ++i) {
    inlineMagicBuilders[types::kIntType->getId()][fmt::format(FMT_STRING("Int{}"), i)] =
        [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
      auto *type = typeRealizations[types::kIntType->getId()];
      return builder.CreateSExtOrTrunc(args[0], type);
    };
    inlineMagicBuilders[types::kIntType->getId()][fmt::format(FMT_STRING("UInt{}"),
                                                              i)] =
        [this](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
      auto *type = typeRealizations[types::kIntType->getId()];
      return builder.CreateZExtOrTrunc(args[0], type);
    };
  }

  auto &llvmCtx = getLLVMContext();
  nonInlineMagicFuncs[types::kStringType->getId()] = {};
  {
    const auto llvmName =
        fmt::format(FMT_STRING("seq.{}{}.memmove"), types::kStringType->getName(),
                    types::kStringType->getId());
    auto *func = cast<Function>(module->getOrInsertFunction(
        llvmName, llvm::Type::getVoidTy(llvmCtx), IntegerType::getInt8PtrTy(llvmCtx),
        IntegerType::getInt8PtrTy(llvmCtx), seqIntLLVM(llvmCtx)));
    func->setDoesNotThrow();
    func->setLinkage(GlobalValue::PrivateLinkage);
    func->addFnAttr(llvm::Attribute::AlwaysInline);
    auto iter = func->arg_begin();
    Value *dst = iter++;
    Value *src = iter++;
    Value *len = iter;
    llvm::BasicBlock *block = llvm::BasicBlock::Create(llvmCtx, "entry", func);
    makeMemMove(dst, src, len, block);
    IRBuilder<> builder(block);
    builder.CreateRetVoid();

    nonInlineMagicFuncs[types::kStringType->getId()]
                       ["memmove[Pointer[byte], Pointer[byte], int]"] = func;
  }
  {
    const auto llvmName =
        fmt::format(FMT_STRING("seq.{}{}.memcpy"), types::kStringType->getName(),
                    types::kStringType->getId());
    auto *func = cast<Function>(module->getOrInsertFunction(
        llvmName, llvm::Type::getVoidTy(llvmCtx), IntegerType::getInt8PtrTy(llvmCtx),
        IntegerType::getInt8PtrTy(llvmCtx), seqIntLLVM(llvmCtx)));
    func->setDoesNotThrow();
    func->setLinkage(GlobalValue::PrivateLinkage);
    func->addFnAttr(llvm::Attribute::AlwaysInline);
    auto iter = func->arg_begin();
    Value *dst = iter++;
    Value *src = iter++;
    Value *len = iter;
    llvm::BasicBlock *block = llvm::BasicBlock::Create(llvmCtx, "entry", func);
    makeMemCpy(dst, src, len, block);
    IRBuilder<> builder(block);
    builder.CreateRetVoid();

    nonInlineMagicFuncs[types::kStringType->getId()]
                       ["memcpy[Pointer[byte], Pointer[byte], int]"] = func;
  }
  {
    const auto llvmName =
        fmt::format(FMT_STRING("seq.{}{}.memset"), types::kStringType->getName(),
                    types::kStringType->getId());
    auto *func = cast<Function>(module->getOrInsertFunction(
        llvmName, llvm::Type::getVoidTy(llvmCtx), IntegerType::getInt8PtrTy(llvmCtx),
        IntegerType::getInt8PtrTy(llvmCtx), seqIntLLVM(llvmCtx)));
    func->setDoesNotThrow();
    func->setLinkage(GlobalValue::PrivateLinkage);
    func->addFnAttr(llvm::Attribute::AlwaysInline);
    auto iter = func->arg_begin();
    Value *dst = iter++;
    Value *val = iter++;
    Value *len = iter;
    llvm::BasicBlock *block = llvm::BasicBlock::Create(llvmCtx, "entry", func);
    IRBuilder<> builder(block);
    builder.CreateMemSet(dst, val, len, 0);
    builder.CreateRetVoid();

    nonInlineMagicFuncs[types::kStringType->getId()]
                       ["memset[Pointer[byte], byte, int]"] = func;
  }

  nonInlineMagicFuncs[types::kBoolType->getId()] = {};
  nonInlineMagicFuncs[types::kFloatType->getId()] = {};
  nonInlineMagicFuncs[types::kIntType->getId()] = {};
  nonInlineMagicFuncs[types::kByteType->getId()] = {};
  {
    const auto name =
        fmt::format(FMT_STRING("seq.{}{}.byte_comp"), types::kByteType->getName(),
                    types::kByteType->getId());
    GlobalVariable *table = getByteCompTable(module);

    auto *func = cast<Function>(
        module->getOrInsertFunction(name, typeRealizations[types::kByteType->getId()],
                                    typeRealizations[types::kByteType->getId()]));
    func->setDoesNotThrow();
    func->setLinkage(GlobalValue::PrivateLinkage);
    func->addFnAttr(llvm::Attribute::AlwaysInline);
    Value *arg = func->arg_begin();
    llvm::BasicBlock *block = llvm::BasicBlock::Create(llvmCtx, "entry", func);
    IRBuilder<> builder(block);
    arg = builder.CreateZExt(arg, builder.getInt64Ty());
    arg = builder.CreateInBoundsGEP(table, {builder.getInt64(0), arg});
    arg = builder.CreateLoad(arg);
    builder.CreateRet(arg);
    nonInlineMagicFuncs[types::kStringType->getId()]["comp[byte, byte]"] = func;
  }
}

} // namespace codegen
} // namespace ir
} // namespace seq
