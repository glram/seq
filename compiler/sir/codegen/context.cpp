#include "context.h"

#include <utility>

#include "sir/bblock.h"
#include "sir/func.h"
#include "sir/trycatch.h"
#include "sir/types/types.h"
#include "sir/var.h"

#include "util/common.h"
#include "util/fmt/format.h"

#include "util.h"

namespace seq {
namespace ir {
namespace codegen {

using namespace llvm;

void TryCatchMetadata::storeDstValue(llvm::BasicBlock *dst,
                                     IRBuilder<> &builder) const {
  ConstantInt *val;
  val = finallyBr->findCaseDest(dst);

  if (!val) {
    val = ConstantInt::get(seqIntLLVM(builder.getContext()), finallyBr->getNumCases());
    finallyBr->addCase(val, dst);
  }

  builder.CreateStore(val, excFlag);
}

Value *TypeRealization::extractMember(Value *self, const std::string &field,
                                      IRBuilder<> &builder) const {
  auto it = fields.find(field);
  if (it == fields.end())
    throw std::runtime_error(fmt::format(FMT_STRING("field {} is not in {}"), field,
                                         irType->referenceString()));
  auto it2 = customGetters.find(field);
  if (it2 != customGetters.end())
    return it2->second(self, builder);
  return builder.CreateExtractValue(self, it->second);
}

Value *TypeRealization::getMemberPointer(Value *ptr, const std::string &field,
                                         IRBuilder<> &builder) const {
  auto it = fields.find(field);
  if (it == fields.end())
    throw std::runtime_error(fmt::format(FMT_STRING("field {} is not in {}"), field,
                                         irType->referenceString()));

  if (memberPointerFunc)
    return memberPointerFunc(ptr, it->second, builder);

  auto *index = ConstantInt::get(seqIntLLVM(builder.getContext()), it->second);
  return builder.CreateGEP(ptr, index);
}

Value *TypeRealization::getDefaultValue(IRBuilder<> &builder) const {
  return dfltBuilder(builder);
}

Value *TypeRealization::getUndefValue() const { return UndefValue::get(llvmType); }

Value *TypeRealization::callMagic(const std::string &sig, std::vector<Value *> args,
                                  IRBuilder<> &builder) {
  auto it = inlineMagicFuncs.find(sig);
  if (it != inlineMagicFuncs.end())
    return it->second(std::move(args), builder);
  throw std::runtime_error(
      fmt::format(FMT_STRING("magic {} is not in {}"), sig, irType->referenceString()));
}

TypeRealization::NonInlineMagicBuilder
TypeRealization::getMagicBuilder(const std::string &sig) const {
  {
    auto it = inlineMagicFuncs.find(sig);
    if (it != inlineMagicFuncs.end()) {
      auto inlineFunc = it->second;
      return [inlineFunc](Function *func) {
        auto *entry = llvm::BasicBlock::Create(func->getContext(), "entry", func);
        IRBuilder<> builder(entry);
        std::vector<llvm::Value *> args;
        for (auto it = func->arg_begin(); it != func->arg_end(); it++) {
          args.push_back(it);
        }
        auto *val = inlineFunc(args, builder);
        val ? builder.CreateRet(val) : builder.CreateRetVoid();
      };
    }
  }
  {
    auto it = nonInlineMagicFuncs.find(sig);
    if (it != nonInlineMagicFuncs.end())
      return it->second;
  }
  throw std::runtime_error(
      fmt::format(FMT_STRING("magic {} is not in {}"), sig, irType->referenceString()));
}

Value *TypeRealization::makeNew(std::vector<llvm::Value *> args,
                                llvm::IRBuilder<> &builder) const {
  return maker ? maker(std::move(args), builder) : dfltBuilder(builder);
}

Value *TypeRealization::load(llvm::Value *ptr, llvm::IRBuilder<> &builder) const {
  return customLoader ? customLoader(ptr, builder) : builder.CreateLoad(ptr);
}

Value *TypeRealization::alloc(Value *count, llvm::IRBuilder<> &builder,
                              bool stack) const {
  if (stack) {
    return builder.CreateAlloca(llvmType, count);
  } else {
    auto *allocFn = makeAllocFunc(builder.GetInsertBlock()->getModule(), false);
    auto *numBytes = builder.CreateMul(ConstantExpr::getSizeOf(llvmType), count);
    return builder.CreateBitCast(builder.CreateCall(allocFn, numBytes), llvmType);
  }
}

Value *TypeRealization::alloc(llvm::IRBuilder<> &builder, bool stack) const {
  return alloc(ConstantInt::get(seqIntLLVM(builder.getContext()), 1), builder, stack);
}

void Context::registerType(std::shared_ptr<types::Type> sirType,
                           std::shared_ptr<TypeRealization> t) {
  typeRealizations[sirType->getId()] = std::move(t);
}

std::shared_ptr<TypeRealization>
Context::getTypeRealization(std::shared_ptr<types::Type> sirType) {
  auto id = sirType->getId();
  if (typeRealizations.find(id) == typeRealizations.end())
    return nullptr;
  return typeRealizations[id];
}

void Context::stubBuiltin(std::shared_ptr<Func> sirFunc,
                          std::shared_ptr<BuiltinStub> stub) {
  builtinStubs[sirFunc->getMagicName()] = std::move(stub);
}

std::shared_ptr<BuiltinStub> Context::getBuiltinStub(const std::string &name) {
  if (builtinStubs.find(name) == builtinStubs.end())
    return nullptr;
  return builtinStubs[name];
}

void Context::registerTryCatch(std::shared_ptr<TryCatch> tc,
                               std::shared_ptr<TryCatchMetadata> meta) {
  auto &frame = getFrame();
  frame.tryCatchMeta[tc->getId()] = std::move(meta);
}
std::shared_ptr<TryCatchMetadata>
Context::getTryCatchMeta(std::shared_ptr<TryCatch> tc) {
  auto &frame = getFrame();
  auto it = frame.tryCatchMeta.find(tc->getId());
  return it == frame.tryCatchMeta.end() ? nullptr : it->second;
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

void Context::initTypeRealizations() {
  auto &llvmCtx = getLLVMContext();

  auto *strType =
      StructType::get(seqIntLLVM(llvmCtx), IntegerType::getInt8PtrTy(llvmCtx));
  auto *boolType = IntegerType::getInt8Ty(llvmCtx);
  auto *floatType = Type::getDoubleTy(llvmCtx);
  auto *intType = seqIntLLVM(llvmCtx);
  auto *byteType = IntegerType::getInt8Ty(llvmCtx);

  {
    auto dfltBuilder = [this, strType](IRBuilder<> &builder) -> Value * {
      Value *self = UndefValue::get(strType);
      self = builder.CreateInsertValue(
          self, ConstantInt::get(seqIntLLVM(getLLVMContext()), 0), 0);
      self = builder.CreateInsertValue(
          self, ConstantPointerNull::get(IntegerType::getInt8PtrTy(getLLVMContext())),
          1);
      return self;
    };

    TypeRealization::InlineMagics inlineMagicFuncs = {
        {"__new__[Pointer[byte], int]",
         [this, strType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           auto *self = UndefValue::get(strType);
           builder.CreateInsertValue(self, args[1], 0);
           builder.CreateInsertValue(self, args[0], 1);
           return self;
         }},
    };
    TypeRealization::NonInlineMagics nonInlineMagicFuncs = {
        {"memmove[Pointer[byte], Pointer[byte], int]",
         [this](llvm::Function *func) {
           auto &llvmCtx = getLLVMContext();
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
         }},
        {"memcpy[Pointer[byte], Pointer[byte], int]",
         [this](llvm::Function *func) {
           auto &llvmCtx = getLLVMContext();
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
         }},
        {"memset[Pointer[byte], byte, int]",
         [this](llvm::Function *func) {
           auto &llvmCtx = getLLVMContext();
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
         }},
    };

    TypeRealization::Fields fields = {{"len", 0}, {"ptr", 1}};
    registerType(types::kStringType,
                 std::make_shared<TypeRealization>(
                     types::kStringType, strType, dfltBuilder, inlineMagicFuncs,
                     "__new__[Pointer[byte], int]", nonInlineMagicFuncs, fields));
  }
  {
    auto dfltBuilder = [boolType](IRBuilder<> &) -> Value * {
      return ConstantInt::get(boolType, 0);
    };
    TypeRealization::InlineMagics inlineMagicFuncs = {
        {"__new__[]",
         [dfltBuilder](std::vector<Value *>, IRBuilder<> &builder) -> Value * {
           return dfltBuilder(builder);
         }},
        {"__str__[bool]",
         [this, strType, boolType](std::vector<Value *> args,
                                   IRBuilder<> &builder) -> Value * {
           auto *self = args[0];
           auto *strFunc = cast<Function>(
               module->getOrInsertFunction("seq_str_bool", strType, boolType));
           strFunc->setDoesNotThrow();
           return builder.CreateCall(strFunc, self);
         }},
        {"__copy__[bool]",
         [](std::vector<Value *> args, IRBuilder<> &) -> Value * { return args[0]; }},
        {"__bool__[bool]",
         [](std::vector<Value *> args, IRBuilder<> &) -> Value * { return args[0]; }},
        {"__invert__[bool]",
         [boolType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateZExt(
               builder.CreateNot(builder.CreateTrunc(
                   args[0], IntegerType::getInt1Ty(builder.getContext()))),
               boolType);
         }},
        {"__eq__[bool, bool]",
         [boolType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateZExt(builder.CreateICmpEQ(args[0], args[1]), boolType);
         }},
        {"__ne__[bool, bool]",
         [boolType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateZExt(builder.CreateICmpNE(args[0], args[1]), boolType);
         }},
        {"__lt__[bool, bool]",
         [boolType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateZExt(builder.CreateICmpULT(args[0], args[1]), boolType);
         }},
        {"__gt__[bool, bool]",
         [boolType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateZExt(builder.CreateICmpUGT(args[0], args[1]), boolType);
         }},
        {"__le__[bool, bool]",
         [boolType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateZExt(builder.CreateICmpULE(args[0], args[1]), boolType);
         }},
        {"__ge__[bool, bool]",
         [boolType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateZExt(builder.CreateICmpUGE(args[0], args[1]), boolType);
         }},
        {"__xor__[bool, bool]",
         [](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateXor(args[0], args[1]);
         }},
    };
    registerType(types::kBoolType,
                 std::make_shared<TypeRealization>(types::kBoolType, boolType,
                                                   dfltBuilder, inlineMagicFuncs));
  }

  {
    auto dfltBuilder = [floatType](IRBuilder<> &) -> Value * {
      return ConstantFP::get(floatType, 0.0);
    };
    TypeRealization::InlineMagics inlineMagicFuncs = {
        {"__new__[]",
         [dfltBuilder](std::vector<Value *>, IRBuilder<> &builder) -> Value * {
           return dfltBuilder(builder);
         }},
        {"__new__[float]",
         [](std::vector<Value *> args, IRBuilder<> &) -> Value * { return args[0]; }},
        {"__new__[int]",
         [floatType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateSIToFP(args[0], floatType);
         }},
        {"__str__[float]",
         [this, floatType, strType](std::vector<Value *> args,
                                    IRBuilder<> &builder) -> Value * {
           auto *self = args[0];
           auto *strFunc = cast<Function>(
               module->getOrInsertFunction("seq_str_float", strType, floatType));
           strFunc->setDoesNotThrow();
           return builder.CreateCall(strFunc, self);
         }},
        {"__copy__[float]",
         [](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return args[0];
         }},
        {"__bool__[float]",
         [floatType, boolType](std::vector<Value *> args,
                               IRBuilder<> &builder) -> Value * {
           return builder.CreateZExt(
               builder.CreateFCmpONE(args[0], ConstantFP::get(floatType, 0.0)),
               boolType);
         }},
        {"__pos__[float]",
         [](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return args[0];
         }},
        {"__neg__[float]",
         [](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateFNeg(args[0]);
         }},
        // float-float binary
        {"__add__[float, float]",
         [](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateFAdd(args[0], args[1]);
         }},
        {"__sub__[float, float]",
         [](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateFSub(args[0], args[1]);
         }},
        {"__mul__[float, float]",
         [](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateFMul(args[0], args[1]);
         }},
        {"__div__[float, float]",
         [floatType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           Value *v = builder.CreateFDiv(args[0], args[1]);
           Function *floor = Intrinsic::getDeclaration(
               builder.GetInsertBlock()->getModule(), Intrinsic::floor, {floatType});
           return builder.CreateCall(floor, v);
         }},
        {"__truediv__[float, float]",
         [](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateFDiv(args[0], args[1]);
         }},
        {"__mod__[float, float]",
         [](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateFRem(args[0], args[1]);
         }},
        {"__pow__[float, float]",
         [floatType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           Function *pow = Intrinsic::getDeclaration(
               builder.GetInsertBlock()->getModule(), Intrinsic::pow, {floatType});
           return builder.CreateCall(pow, {args[0], args[1]});
         }},
        {"__eq__[float, float]",
         [boolType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateZExt(builder.CreateFCmpOEQ(args[0], args[1]), boolType);
         }},
        {"__ne__[float, float]",
         [boolType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateZExt(builder.CreateFCmpONE(args[0], args[1]), boolType);
         }},
        {"__lt__[float, float]",
         [boolType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateZExt(builder.CreateFCmpOLT(args[0], args[1]), boolType);
         }},
        {"__gt__[float, float]",
         [boolType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateZExt(builder.CreateFCmpOGT(args[0], args[1]), boolType);
         }},
        {"__le__[float, float]",
         [boolType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateZExt(builder.CreateFCmpOLE(args[0], args[1]), boolType);
         }},
        {"__ge__[float, float]",
         [boolType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateZExt(builder.CreateFCmpOGE(args[0], args[1]), boolType);
         }},
        // float-int
        {"__add__[float, int]",
         [floatType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           args[1] = builder.CreateSIToFP(args[1], floatType);
           return builder.CreateFAdd(args[0], args[1]);
         }},
        {"__sub__[float, int]",
         [floatType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           args[1] = builder.CreateSIToFP(args[1], floatType);
           return builder.CreateFSub(args[0], args[1]);
         }},
        {"__mul__[float, int]",
         [floatType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           args[1] = builder.CreateSIToFP(args[1], floatType);
           return builder.CreateFMul(args[0], args[1]);
         }},
        {"__div__[float, int]",
         [floatType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           args[1] = builder.CreateSIToFP(args[1], floatType);
           Value *v = builder.CreateFDiv(args[0], args[1]);
           Function *floor = Intrinsic::getDeclaration(
               builder.GetInsertBlock()->getModule(), Intrinsic::floor, {floatType});
           return builder.CreateCall(floor, v);
         }},
        {"__truediv__[float, int]",
         [floatType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           args[1] = builder.CreateSIToFP(args[1], floatType);
           return builder.CreateFDiv(args[0], args[1]);
         }},
        {"__mod__[float, int]",
         [floatType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           args[1] = builder.CreateSIToFP(args[1], floatType);
           return builder.CreateFRem(args[0], args[1]);
         }},
        {"__pow__[float, int]",
         [floatType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           args[1] = builder.CreateSIToFP(args[1], floatType);
           Function *pow = Intrinsic::getDeclaration(
               builder.GetInsertBlock()->getModule(), Intrinsic::pow, {floatType});
           return builder.CreateCall(pow, {args[0], args[1]});
         }},
        {"__eq__[float, int]",
         [floatType, boolType](std::vector<Value *> args,
                               IRBuilder<> &builder) -> Value * {
           args[1] = builder.CreateSIToFP(args[1], floatType);
           return builder.CreateZExt(builder.CreateFCmpOEQ(args[0], args[1]), boolType);
         }},
        {"__ne__[float, int]",
         [floatType, boolType](std::vector<Value *> args,
                               IRBuilder<> &builder) -> Value * {
           args[1] = builder.CreateSIToFP(args[1], floatType);
           return builder.CreateZExt(builder.CreateFCmpONE(args[0], args[1]), boolType);
         }},
        {"__lt__[float, int]",
         [floatType, boolType](std::vector<Value *> args,
                               IRBuilder<> &builder) -> Value * {
           args[1] = builder.CreateSIToFP(args[1], floatType);
           return builder.CreateZExt(builder.CreateFCmpOLT(args[0], args[1]), boolType);
         }},
        {"__gt__[float, int]",
         [floatType, boolType](std::vector<Value *> args,
                               IRBuilder<> &builder) -> Value * {
           args[1] = builder.CreateSIToFP(args[1], floatType);
           return builder.CreateZExt(builder.CreateFCmpOGT(args[0], args[1]), boolType);
         }},
        {"__le__[float, int]",
         [floatType, boolType](std::vector<Value *> args,
                               IRBuilder<> &builder) -> Value * {
           args[1] = builder.CreateSIToFP(args[1], floatType);
           return builder.CreateZExt(builder.CreateFCmpOLE(args[0], args[1]), boolType);
         }},
        {"__ge__[float, int]",
         [floatType, boolType](std::vector<Value *> args,
                               IRBuilder<> &builder) -> Value * {
           args[1] = builder.CreateSIToFP(args[1], floatType);
           return builder.CreateZExt(builder.CreateFCmpOGE(args[0], args[1]), boolType);
         }},
    };
    registerType(types::kFloatType,
                 std::make_shared<TypeRealization>(types::kFloatType, floatType,
                                                   dfltBuilder, inlineMagicFuncs));
  }

  {
    auto dfltBuilder = [intType](IRBuilder<> &) -> Value * {
      return ConstantInt::get(intType, 0);
    };
    TypeRealization::InlineMagics inlineMagicFuncs = {
        {"__new__[]",
         [dfltBuilder](std::vector<Value *>, IRBuilder<> &builder) -> Value * {
           return dfltBuilder(builder);
         }},
        {"__new__[int]",
         [](std::vector<Value *> args, IRBuilder<> &) -> Value * { return args[0]; }},
        {"__new__[float]",
         [intType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateFPToSI(args[0], intType);
         }},
        {"__new__[bool]",
         [intType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateZExt(args[0], intType);
         }},
        {"__new__[byte]",
         [intType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateZExt(args[0], intType);
         }},
        {"__str__[int]",
         [this, intType, strType](std::vector<Value *> args,
                                  IRBuilder<> &builder) -> Value * {
           auto *self = args[0];
           auto *strFunc = cast<Function>(
               module->getOrInsertFunction("seq_str_int", strType, intType));
           strFunc->setDoesNotThrow();
           return builder.CreateCall(strFunc, self);
         }},
        {"__copy__[int]",
         [](std::vector<Value *> args, IRBuilder<> &) -> Value * { return args[0]; }},
        {"__hash__[int]",
         [](std::vector<Value *> args, IRBuilder<> &) -> Value * { return args[0]; }},
        {"__bool__[int]",
         [intType, boolType](std::vector<Value *> args,
                             IRBuilder<> &builder) -> Value * {
           return builder.CreateZExt(
               builder.CreateICmpEQ(args[0], ConstantInt::get(intType, 0)), boolType);
         }},
        {"__pos__[int]",
         [](std::vector<Value *> args, IRBuilder<> &) -> Value * { return args[0]; }},
        {"__neg__[int]",
         [](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateNeg(args[0]);
         }},
        // int-int binary
        {"__add__[int, int]",
         [](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateAdd(args[0], args[1]);
         }},
        {"__sub__[int, int]",
         [](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateSub(args[0], args[1]);
         }},
        {"__mul__[int, int]",
         [](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateMul(args[0], args[1]);
         }},
        {"__truediv__[int, int]",
         [floatType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           args[0] = builder.CreateSIToFP(args[0], floatType);
           args[1] = builder.CreateSIToFP(args[1], floatType);
           return builder.CreateFDiv(args[0], args[1]);
         }},
        {"__div__[int, int]",
         [](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateSDiv(args[0], args[1]);
         }},
        {"__mod__[int, int]",
         [](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateSRem(args[0], args[1]);
         }},
        {"__lshift__[int, int]",
         [](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateShl(args[0], args[1]);
         }},
        {"__rshift__[int, int]",
         [](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateAShr(args[0], args[1]);
         }},
        {"__eq__[int, int]",
         [boolType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateZExt(builder.CreateICmpEQ(args[0], args[1]), boolType);
         }},
        {"__ne__[int, int]",
         [boolType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateZExt(builder.CreateICmpNE(args[0], args[1]), boolType);
         }},
        {"__lt__[int, int]",
         [boolType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateZExt(builder.CreateICmpSLT(args[0], args[1]), boolType);
         }},
        {"__gt__[int, int]",
         [boolType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateZExt(builder.CreateICmpSGT(args[0], args[1]), boolType);
         }},
        {"__le__[int, int]",
         [boolType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateZExt(builder.CreateICmpSLE(args[0], args[1]), boolType);
         }},
        {"__ge__[int, int]",
         [boolType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateZExt(builder.CreateICmpSLE(args[0], args[1]), boolType);
         }},
        // int-float
        {"__add__[int, float]",
         [floatType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           args[0] = builder.CreateSIToFP(args[0], floatType);
           return builder.CreateFAdd(args[0], args[1]);
         }},
        {"__sub__[int, float]",
         [floatType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           args[0] = builder.CreateSIToFP(args[0], floatType);
           return builder.CreateFSub(args[0], args[1]);
         }},
        {"__mul__[int, float]",
         [floatType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           args[0] = builder.CreateSIToFP(args[0], floatType);
           return builder.CreateFMul(args[0], args[1]);
         }},
        {"__div__[int, float]",
         [floatType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           args[1] = builder.CreateSIToFP(args[1], floatType);
           Value *v = builder.CreateFDiv(args[0], args[1]);
           Function *floor = Intrinsic::getDeclaration(
               builder.GetInsertBlock()->getModule(), Intrinsic::floor, {floatType});
           return builder.CreateCall(floor, v);
         }},
        {"__truediv__[int, float]",
         [floatType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           args[0] = builder.CreateSIToFP(args[0], floatType);
           return builder.CreateFDiv(args[0], args[1]);
         }},
        {"__mod__[int, float]",
         [floatType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           args[0] = builder.CreateSIToFP(args[0], floatType);
           return builder.CreateFRem(args[0], args[1]);
         }},
        {"__pow__[int, float]",
         [floatType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           args[1] = builder.CreateSIToFP(args[1], floatType);
           Function *pow = Intrinsic::getDeclaration(
               builder.GetInsertBlock()->getModule(), Intrinsic::pow, {floatType});
           return builder.CreateCall(pow, {args[0], args[1]});
         }},
        {"__eq__[int, float]",
         [floatType, boolType](std::vector<Value *> args,
                               IRBuilder<> &builder) -> Value * {
           args[0] = builder.CreateSIToFP(args[0], floatType);
           return builder.CreateZExt(builder.CreateFCmpOEQ(args[0], args[1]), boolType);
         }},
        {"__ne__[int, float]",
         [floatType, boolType](std::vector<Value *> args,
                               IRBuilder<> &builder) -> Value * {
           args[0] = builder.CreateSIToFP(args[0], floatType);
           return builder.CreateZExt(builder.CreateFCmpONE(args[0], args[1]), boolType);
         }},
        {"__lt__[int, float]",
         [floatType, boolType](std::vector<Value *> args,
                               IRBuilder<> &builder) -> Value * {
           args[0] = builder.CreateSIToFP(args[0], floatType);
           return builder.CreateZExt(builder.CreateFCmpOLT(args[0], args[1]), boolType);
         }},
        {"__gt__[int, float]",
         [floatType, boolType](std::vector<Value *> args,
                               IRBuilder<> &builder) -> Value * {
           args[0] = builder.CreateSIToFP(args[0], floatType);
           return builder.CreateZExt(builder.CreateFCmpOGT(args[0], args[1]), boolType);
         }},
        {"__le__[int, float]",
         [floatType, boolType](std::vector<Value *> args,
                               IRBuilder<> &builder) -> Value * {
           args[0] = builder.CreateSIToFP(args[0], floatType);
           return builder.CreateZExt(builder.CreateFCmpOLE(args[0], args[1]), boolType);
         }},
        {"__ge__[int, float]",
         [floatType, boolType](std::vector<Value *> args,
                               IRBuilder<> &builder) -> Value * {
           args[0] = builder.CreateSIToFP(args[0], floatType);
           return builder.CreateZExt(builder.CreateFCmpOGE(args[0], args[1]), boolType);
         }},
    };
    for (unsigned i = 1; i <= types::IntNType::MAX_LEN; ++i) {
      inlineMagicFuncs[fmt::format(FMT_STRING("Int{}"), i)] =
          [intType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
        return builder.CreateSExtOrTrunc(args[0], intType);
      };
      inlineMagicFuncs[fmt::format(FMT_STRING("UInt{}"), i)] =
          [intType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
        return builder.CreateZExtOrTrunc(args[0], intType);
      };
    }

    registerType(types::kIntType,
                 std::make_shared<TypeRealization>(types::kIntType, intType,
                                                   dfltBuilder, inlineMagicFuncs));
  }

  {
    auto dfltBuilder = [byteType](IRBuilder<> &) -> Value * {
      return ConstantInt::get(byteType, 0);
    };
    TypeRealization::InlineMagics inlineMagicFuncs = {
        {"__new__[]",
         [dfltBuilder](std::vector<Value *>, IRBuilder<> &builder) -> Value * {
           return dfltBuilder(builder);
         }},
        {"__new__[byte]",
         [](std::vector<Value *> args, IRBuilder<> &) -> Value * { return args[0]; }},
        {"__new__[int]",
         [byteType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateTrunc(args[0], byteType);
         }},
        {"__str__[byte]",
         [this, byteType, strType](std::vector<Value *> args,
                                   IRBuilder<> &builder) -> Value * {
           auto *self = args[0];
           auto *strFunc = cast<Function>(
               module->getOrInsertFunction("seq_str_byte", strType, byteType));
           strFunc->setDoesNotThrow();
           return builder.CreateCall(strFunc, self);
         }},
        {"__copy__[byte]",
         [](std::vector<Value *> args, IRBuilder<> &) -> Value * { return args[0]; }},
        {"__bool__[byte]",
         [byteType, boolType](std::vector<Value *> args,
                              IRBuilder<> &builder) -> Value * {
           Value *zero = ConstantInt::get(byteType, 0);
           return builder.CreateZExt(builder.CreateFCmpONE(args[0], zero), boolType);
         }},
        {"__eq__[byte, byte]",
         [boolType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateZExt(builder.CreateICmpEQ(args[0], args[1]), boolType);
         }},
        {"__ne__[byte, byte]",
         [boolType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateZExt(builder.CreateICmpNE(args[0], args[1]), boolType);
         }},
        {"__lt__[byte, byte]",
         [boolType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateZExt(builder.CreateICmpULT(args[0], args[1]), boolType);
         }},
        {"__gt__[byte, byte]",
         [boolType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateZExt(builder.CreateICmpUGT(args[0], args[1]), boolType);
         }},
        {"__le__[byte, byte]",
         [boolType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateZExt(builder.CreateICmpULE(args[0], args[1]), boolType);
         }},
        {"__ge__[byte, byte]",
         [boolType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
           return builder.CreateZExt(builder.CreateICmpUGE(args[0], args[1]), boolType);
         }},
    };
    TypeRealization::NonInlineMagics nonInlineMagicFuncs = {
        {"comp[byte, byte]", [this](llvm::Function *func) {
           auto &llvmCtx = getLLVMContext();
           GlobalVariable *table = getByteCompTable(module);

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
         }}};
    registerType(types::kByteType, std::make_shared<TypeRealization>(
                                       types::kByteType, byteType, dfltBuilder,
                                       inlineMagicFuncs, "", nonInlineMagicFuncs));
  }
  {
    registerType(types::kVoidType,
                 std::make_shared<TypeRealization>(
                     types::kVoidType, Type::getVoidTy(getLLVMContext()), nullptr,
                     TypeRealization::InlineMagics()));
  }
}

Value *Context::codegenStr(Value *self, const std::string &name,
                           llvm::BasicBlock *block) {
  LLVMContext &context = block->getContext();
  IRBuilder<> builder(block);
  Value *ptr = builder.CreateBitCast(self, builder.getInt8PtrTy());

  auto *nameVar = new GlobalVariable(
      *block->getModule(), llvm::ArrayType::get(builder.getInt8Ty(), name.length() + 1),
      true, GlobalValue::PrivateLinkage, ConstantDataArray::getString(context, name),
      "typename_literal");
  nameVar->setAlignment(1);

  Value *str = builder.CreateBitCast(nameVar, builder.getInt8PtrTy());
  Value *len = ConstantInt::get(seqIntLLVM(context), name.length());

  auto strTypeRealization = getTypeRealization(types::kStringType);
  Value *nameVal = strTypeRealization->makeNew({len, str}, builder);

  auto *fn = getBuiltinStub("_raw_type_str")->func;
  return builder.CreateCall(fn, {ptr, nameVal});
}

} // namespace codegen
} // namespace ir
} // namespace seq
