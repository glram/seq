#include "codegen.h"

#include <algorithm>

#include "common.h"

#include "util/fmt/format.h"

#include "sir/base.h"
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
std::string getLLVMFuncName(std::shared_ptr<seq::ir::Func> f) {
  return fmt::format(FMT_STRING("seq.{}{}"), f->getName(), f->getId());
}
} // namespace

namespace seq {
namespace ir {
namespace codegen {

using namespace llvm;

void CodegenVisitor::transform(std::shared_ptr<IRModule> module) {
  CodegenVisitor v(ctx);
  module->accept(v);
}

void CodegenVisitor::transform(std::shared_ptr<BasicBlock> block) {
  CodegenVisitor v(ctx);
  block->accept(v);
}

Value *CodegenVisitor::transform(std::shared_ptr<Var> var,
                                 const std::string &nameOverride) {
  if (var->isFunc()) {
    auto name = nameOverride.empty()
                    ? getLLVMFuncName(std::static_pointer_cast<Func>(var))
                    : nameOverride;
    auto *llvmFunc = ctx->getModule()->getFunction(name);
    if (llvmFunc)
      return llvmFunc;
  } else {
    auto *val = ctx->getVar(var);
    if (val)
      return val;
  }
  CodegenVisitor v(ctx);
  var->accept(v, nameOverride);
  return v.valResult;
}

void CodegenVisitor::transform(std::shared_ptr<Instr> instr) {
  CodegenVisitor v(ctx);
  instr->accept(v);
}

Value *CodegenVisitor::transform(std::shared_ptr<Rvalue> rval) {
  CodegenVisitor v(ctx);
  rval->accept(v);
  return v.valResult;
}

Value *CodegenVisitor::transform(std::shared_ptr<Lvalue> lval) {
  CodegenVisitor v(ctx);
  lval->accept(v);
  return v.valResult;
}

Value *CodegenVisitor::transform(std::shared_ptr<Operand> op) {
  CodegenVisitor v(ctx);
  op->accept(v);
  return v.valResult;
}

Value *CodegenVisitor::transform(std::shared_ptr<Pattern> pat) {
  CodegenVisitor v(ctx);
  pat->accept(v);
  return v.valResult;
}
void CodegenVisitor::transform(std::shared_ptr<Terminator> term) {
  CodegenVisitor v(ctx);
  term->accept(v);
}

Type *CodegenVisitor::transform(std::shared_ptr<types::Type> typ) {
  auto lookedUpType = ctx->getLLVMType(typ);
  if (lookedUpType)
    return lookedUpType;

  CodegenVisitor v(ctx);
  typ->accept(v);
  return v.typeResult;
}

void CodegenVisitor::visit(std::shared_ptr<IRModule> module) {
  // TODO deal with argvar
  ctx->pushFrame();
  for (auto &g : module->getGlobals())
    transform(g);
  transform(module->getBase(), "seq.main");
  ctx->popFrame();
}

void CodegenVisitor::visit(std::shared_ptr<BasicBlock> block) {
  auto *llvmBlock = ctx->getBlock(block);
  ctx->getFrame().curBlock = llvmBlock;

  for (auto &inst : block->getInstructions())
    transform(inst);

  if (block->getTerminator())
    transform(block->getTerminator());
  else {
    // TODO implicit return/yield
  }
}

void CodegenVisitor::visit(std::shared_ptr<Func> sirFunc,
                           const std::string &nameOverride) {
  if (sirFunc->isExternal())
    return;

  auto *module = ctx->getModule();
  auto sirFuncType = std::static_pointer_cast<types::FuncType>(sirFunc->getType());

  auto *funcPtrType = cast<PointerType>(transform(sirFunc->getType()));
  auto name = nameOverride.empty() ? getLLVMFuncName(sirFunc) : nameOverride;
  auto *func =
      cast<Function>(module->getOrInsertFunction(name, funcPtrType->getElementType()));
  auto *outLLVMType = transform(sirFuncType->getRType());

  valResult = func;

  if (sirFunc->isInternal()) {
    ctx->getMagicBuilder(
        sirFunc->getParentType(),
        getMagicSignature(sirFunc->getMagicName(), sirFuncType->getArgTypes()))(func);
    return;
  }

  auto funcAttributes =
      std::static_pointer_cast<FuncAttribute>(sirFunc->getAttribute(kFuncAttribute));
  auto srcInfoAttribute = std::static_pointer_cast<SrcInfoAttribute>(
      sirFunc->getAttribute(kSrcInfoAttribute));
  auto srcInfo = srcInfoAttribute ? srcInfoAttribute->info : seq::SrcInfo();

  if (funcAttributes) {
    if (funcAttributes->has("export")) {
      if (sirFunc->getEnclosingFunc().lock())
        throw exc::SeqException("can only export top-level functions", srcInfo);
      func->setLinkage(GlobalValue::ExternalLinkage);
    } else {
      func->setLinkage(GlobalValue::PrivateLinkage);
    }

    if (funcAttributes->has("inline")) {
      func->addFnAttr(llvm::Attribute::AlwaysInline);
    }
    if (funcAttributes->has("noinline")) {
      func->addFnAttr(llvm::Attribute::NoInline);
    }
  } else {
    func->setLinkage(GlobalValue::PrivateLinkage);
  }

  // TODO profiling
  func->setPersonalityFn(makePersonalityFunc(module));

  auto &context = ctx->getLLVMContext();

  ctx->pushFrame();
  auto &frame = ctx->getFrame();
  frame.funcMetadata.func = func;
  frame.funcMetadata.isGenerator = sirFunc->isGenerator();

  // Stub blocks
  for (auto &b : sirFunc->getBlocks()) {
    auto blockName = fmt::format(FMT_STRING("bb{}"), b->getId());
    ctx->registerBlock(b, llvm::BasicBlock::Create(context, blockName, func));
  }

  auto *preamble = llvm::BasicBlock::Create(context, "preamble", func);

  frame.curBlock = preamble;
  IRBuilder<> builder(preamble);

  // General generator primitives
  Value *id = nullptr;
  if (sirFunc->isGenerator()) {
    Function *idFn = Intrinsic::getDeclaration(module, Intrinsic::coro_id);
    Value *nullPtr = ConstantPointerNull::get(IntegerType::getInt8PtrTy(context));

    if (sirFuncType->getRType()->getId() != types::kVoidType->getId()) {
      auto *promise = makeAlloca(outLLVMType, preamble);
      promise->setName("promise");
      Value *promiseRaw =
          builder.CreateBitCast(promise, IntegerType::getInt8PtrTy(context));
      id = builder.CreateCall(idFn,
                              {ConstantInt::get(IntegerType::getInt32Ty(context), 0),
                               promiseRaw, nullPtr, nullPtr});
    } else {
      id = builder.CreateCall(idFn,
                              {ConstantInt::get(IntegerType::getInt32Ty(context), 0),
                               nullPtr, nullPtr, nullPtr});
    }
    id->setName("id");
  }

  // Register vars
  for (auto &v : sirFunc->getVars())
    ctx->registerVar(v, transform(v));

  // Register arg vars
  auto argIter = func->arg_begin();
  for (auto &a : sirFunc->getArgVars()) {
    auto *var = transform(a);
    ctx->registerVar(a, var);
    builder.CreateStore(var, argIter++);
  }

  llvm::BasicBlock *allocBlock = nullptr;
  Value *alloc = nullptr;
  if (sirFunc->isGenerator()) {
    allocBlock = llvm::BasicBlock::Create(context, "alloc", func);
    builder.SetInsertPoint(allocBlock);
    Function *sizeFn =
        Intrinsic::getDeclaration(module, Intrinsic::coro_size, {seqIntLLVM(context)});
    Value *size = builder.CreateCall(sizeFn);
    auto *allocFunc = makeAllocFunc(module, false);
    alloc = builder.CreateCall(allocFunc, size);
  }

  llvm::BasicBlock *entry = llvm::BasicBlock::Create(context, "entry", func);
  llvm::BasicBlock *entryActual = entry;
  llvm::BasicBlock *dynFree = nullptr;

  if (sirFunc->isGenerator()) {
    builder.CreateBr(entry);
    builder.SetInsertPoint(entry);
    PHINode *phi = builder.CreatePHI(IntegerType::getInt8PtrTy(context), 2);
    phi->addIncoming(ConstantPointerNull::get(IntegerType::getInt8PtrTy(context)),
                     preamble);
    phi->addIncoming(alloc, allocBlock);

    Function *beginFn = Intrinsic::getDeclaration(module, Intrinsic::coro_begin);
    auto *handle = builder.CreateCall(beginFn, {id, phi});
    handle->setName("hdl");

    frame.funcMetadata.handle = handle;

    /*
     * Cleanup code
     */
    auto *cleanup = llvm::BasicBlock::Create(context, "cleanup", func);
    frame.funcMetadata.cleanup = cleanup;

    dynFree = llvm::BasicBlock::Create(context, "dyn_free", func);
    builder.SetInsertPoint(cleanup);
    Function *freeFn = Intrinsic::getDeclaration(module, Intrinsic::coro_free);
    Value *mem = builder.CreateCall(freeFn, {id, handle});
    Value *needDynFree = builder.CreateIsNotNull(mem);

    auto *suspend = llvm::BasicBlock::Create(context, "suspend", func);
    frame.funcMetadata.suspend = suspend;
    builder.CreateCondBr(needDynFree, dynFree, suspend);

    builder.SetInsertPoint(dynFree);
    builder.CreateBr(suspend);

    builder.SetInsertPoint(suspend);
    Function *endFn = Intrinsic::getDeclaration(module, Intrinsic::coro_end);
    builder.CreateCall(endFn,
                       {handle, ConstantInt::get(IntegerType::getInt1Ty(context), 0)});
    builder.CreateRet(handle);

    frame.funcMetadata.exit = llvm::BasicBlock::Create(context, "final", func);
    funcYield(frame.funcMetadata, nullptr, frame.funcMetadata.exit, nullptr);
  }

  if (sirFunc->isGenerator()) {
    auto *newEntry = llvm::BasicBlock::Create(context, "actualEntry", func);
    funcYield(frame.funcMetadata, nullptr, entry, newEntry);
    entry = newEntry;
  }

  for (auto &b : sirFunc->getBlocks())
    transform(b);

  builder.SetInsertPoint(entry);
  builder.CreateBr(ctx->getBlock(sirFunc->getBlocks().front()));

  builder.SetInsertPoint(preamble);
  if (sirFunc->isGenerator()) {
    Function *allocFn = Intrinsic::getDeclaration(module, Intrinsic::coro_alloc);
    Value *needAlloc = builder.CreateCall(allocFn, id);
    builder.CreateCondBr(needAlloc, allocBlock, entryActual);

    frame.funcMetadata.exit->moveAfter(&func->getBasicBlockList().back());
    frame.funcMetadata.cleanup->moveAfter(frame.funcMetadata.exit);
    dynFree->moveAfter(frame.funcMetadata.cleanup);
    frame.funcMetadata.suspend->moveAfter(dynFree);
  } else {
    builder.CreateBr(entry);
  }

  ctx->popFrame();
}

void CodegenVisitor::visit(std::shared_ptr<Var> var, const std::string &) {
  // TODO: repl!
  auto *llvmType = transform(var->getType());
  Value *val;
  if (var->isGlobal()) {
    val = new GlobalVariable(*ctx->getModule(), llvmType, false,
                             GlobalValue::PrivateLinkage,
                             Constant::getNullValue(llvmType), var->getName());
  } else {
    IRBuilder<> builder(ctx->getFrame().curBlock);
    val = builder.CreateAlloca(
        llvmType, llvm::ConstantInt::get(seqIntLLVM(ctx->getLLVMContext()), 1));
  }
  ctx->registerVar(var, val);
}

void CodegenVisitor::visit(std::shared_ptr<AssignInstr> assignInstr) {
  IRBuilder<> builder(ctx->getFrame().curBlock);
  builder.CreateStore(transform(assignInstr->getLhs()),
                      transform(assignInstr->getRhs()));
}

void CodegenVisitor::visit(std::shared_ptr<RvalueInstr> rvalInstr) {
  transform(rvalInstr->getRvalue());
}

void CodegenVisitor::visit(std::shared_ptr<MemberRvalue> memberRval) {
  auto aggType =
      std::static_pointer_cast<types::MemberedType>(memberRval->getVar()->getType());
  auto aggLLVMType = transform(aggType);

  auto memberNames = aggType->getMemberNames();
  auto fieldIndex =
      std::find(memberNames.begin(), memberNames.end(), memberRval->getField()) -
      memberNames.begin();

  IRBuilder<> builder(ctx->getFrame().curBlock);
  auto *val = transform(memberRval->getVar());
  if (aggType->isRef()) {
    auto contentType = std::static_pointer_cast<types::RefType>(aggType)->getContents();
    aggLLVMType = transform(contentType);
    val = builder.CreateBitCast(val, aggLLVMType->getPointerTo());
    val = builder.CreateLoad(val);
  }

  valResult = builder.CreateExtractValue(val, fieldIndex);
}

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
    nonInlineMagicFuncs[getMagicSignature(
        "__getitem__", {memberedType, types::kIntType})] = [=](Function *func) {
      func->setLinkage(GlobalValue::PrivateLinkage);
      llvm::Type *baseType = body[0];

      auto iter = func->arg_begin();
      Value *self = iter++;
      Value *idx = iter;
      llvm::BasicBlock *entry =
          llvm::BasicBlock::Create(ctx->getLLVMContext(), "entry", func);

      IRBuilder<> b(entry);
      b.SetInsertPoint(entry);
      Value *ptr = b.CreateAlloca(llvmType);
      b.CreateStore(self, ptr);
      ptr = b.CreateBitCast(ptr, baseType->getPointerTo());
      ptr = b.CreateGEP(ptr, idx);
      b.CreateRet(b.CreateLoad(ptr));
    };
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

  auto callArgs = funcType->getArgTypes();
  callArgs.insert(callArgs.begin(), funcType);

  Context::InlineMagicFuncs inlineMagicFuncs = {
      {"__new__[Pointer[byte]]",
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateBitCast(args[0], llvmType);
       }},
      {getMagicSignature("__str__", {funcType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return codegenStr(args[0], "generator", builder.GetInsertBlock());
       }},
      {getMagicSignature("__call__", callArgs),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateCall(args[0],
                                   std::vector<Value *>(args.begin() + 1, args.end()));
       }},
  };
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
        self, ConstantPointerNull::get(PointerType::get(baseLLVMType, 0)), 1);
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
      {getMagicSignature("__prefetch_r0__", {pointerType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         auto *self = builder.CreateBitCast(args[0], builder.getInt8PtrTy());
         Function *prefetch =
             Intrinsic::getDeclaration(ctx->getModule(), Intrinsic::prefetch);
         builder.CreateCall(prefetch, {self, builder.getInt32(0), builder.getInt32(0),
                                       builder.getInt32(1)});
         return (Value *)nullptr;
       }},

      /*
       * Prefetch magics are labeled [rw][0123] representing read/write and
       * locality. Instruction cache prefetch is not supported.
       * https://llvm.org/docs/LangRef.html#llvm-prefetch-intrinsic
       */
      {getMagicSignature("__prefetch_r1__", {pointerType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         auto *self = builder.CreateBitCast(args[0], builder.getInt8PtrTy());
         Function *prefetch =
             Intrinsic::getDeclaration(ctx->getModule(), Intrinsic::prefetch);
         builder.CreateCall(prefetch, {self, builder.getInt32(0), builder.getInt32(1),
                                       builder.getInt32(1)});
         return (Value *)nullptr;
       }},
      {getMagicSignature("__prefetch_r2__", {pointerType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         auto *self = builder.CreateBitCast(args[0], builder.getInt8PtrTy());
         Function *prefetch =
             Intrinsic::getDeclaration(ctx->getModule(), Intrinsic::prefetch);
         builder.CreateCall(prefetch, {self, builder.getInt32(0), builder.getInt32(2),
                                       builder.getInt32(1)});
         return (Value *)nullptr;
       }},
      {getMagicSignature("__prefetch_r3__", {pointerType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         auto *self = builder.CreateBitCast(args[0], builder.getInt8PtrTy());
         Function *prefetch =
             Intrinsic::getDeclaration(ctx->getModule(), Intrinsic::prefetch);
         builder.CreateCall(prefetch, {self, builder.getInt32(0), builder.getInt32(3),
                                       builder.getInt32(1)});
         return (Value *)nullptr;
       }},
      {getMagicSignature("__prefetch_w0__", {pointerType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         auto *self = builder.CreateBitCast(args[0], builder.getInt8PtrTy());
         Function *prefetch =
             Intrinsic::getDeclaration(ctx->getModule(), Intrinsic::prefetch);
         builder.CreateCall(prefetch, {self, builder.getInt32(1), builder.getInt32(0),
                                       builder.getInt32(1)});
         return (Value *)nullptr;
       }},
      {getMagicSignature("__prefetch_w1__", {pointerType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         auto *self = builder.CreateBitCast(args[0], builder.getInt8PtrTy());
         Function *prefetch =
             Intrinsic::getDeclaration(ctx->getModule(), Intrinsic::prefetch);
         builder.CreateCall(prefetch, {self, builder.getInt32(1), builder.getInt32(1),
                                       builder.getInt32(1)});
         return (Value *)nullptr;
       }},
      {getMagicSignature("__prefetch_w2__", {pointerType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         auto *self = builder.CreateBitCast(args[0], builder.getInt8PtrTy());
         Function *prefetch =
             Intrinsic::getDeclaration(ctx->getModule(), Intrinsic::prefetch);
         builder.CreateCall(prefetch, {self, builder.getInt32(1), builder.getInt32(2),
                                       builder.getInt32(1)});
         return (Value *)nullptr;
       }},
      {getMagicSignature("__prefetch_w3__", {pointerType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         auto *self = builder.CreateBitCast(args[0], builder.getInt8PtrTy());
         Function *prefetch =
             Intrinsic::getDeclaration(ctx->getModule(), Intrinsic::prefetch);
         builder.CreateCall(prefetch, {self, builder.getInt32(1), builder.getInt32(3),
                                       builder.getInt32(1)});
         return (Value *)nullptr;
       }},
  };
  ctx->registerType(pointerType, llvmType, dfltBuilder, inlineMagics, {});
  typeResult = llvmType;
}

void CodegenVisitor::visit(std::shared_ptr<types::Generator> genType) {
  auto baseType = genType->getBase();
  auto *baseLLVMType = transform(baseType);

  auto *llvmType = IntegerType::getInt8PtrTy(ctx->getLLVMContext());

  auto dfltBuilder = [=](IRBuilder<> &builder) -> Value * {
    return ConstantPointerNull::get(llvmType);
  };

  Context::InlineMagicFuncs inlineMagics = {
      {getMagicSignature("__iter__", {genType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return args[0];
       }},
      {getMagicSignature("__raw__", {genType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return args[0];
       }},
      {getMagicSignature("__done__", {genType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateZExt(generatorDone(args[0], builder.GetInsertBlock()),
                                   ctx->getLLVMType(types::kBoolType));
       }},
      {getMagicSignature("done", {genType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         generatorResume(args[0], builder.GetInsertBlock(), nullptr, nullptr);
         return builder.CreateZExt(generatorDone(args[0], builder.GetInsertBlock()),
                                   ctx->getLLVMType(types::kBoolType));
       }},
      {getMagicSignature("__promise__", {genType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return generatorPromise(args[0], builder.GetInsertBlock(), baseLLVMType, true);
       }},
      {getMagicSignature("__resume__", {genType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         generatorResume(args[0], builder.GetInsertBlock(), nullptr, nullptr);
         return nullptr;
       }},
      {getMagicSignature("send", {genType, baseType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         generatorSend(args[0], args[1], builder.GetInsertBlock(), baseLLVMType);
         generatorResume(args[0], builder.GetInsertBlock(), nullptr, nullptr);
         return generatorPromise(args[0], builder.GetInsertBlock(), baseLLVMType);
       }},
      {getMagicSignature("__str__", {genType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return codegenStr(args[0], "generator", builder.GetInsertBlock());
       }},
      {getMagicSignature("destroy", {genType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         generatorDestroy(args[0], builder.GetInsertBlock());
       }},
  };

  Context::NonInlineMagicFuncs nonInlineMagicFuncs = {
      {getMagicSignature("next", {genType}), [=](Function *func) {
         func->setLinkage(GlobalValue::PrivateLinkage);
         func->setDoesNotThrow();
         func->setPersonalityFn(makePersonalityFunc(ctx->getModule()));
         func->addFnAttr(llvm::Attribute::AlwaysInline);

         Value *arg = func->arg_begin();
         auto *entry = llvm::BasicBlock::Create(ctx->getLLVMContext(), "entry", func);
         auto *val = generatorPromise(arg, entry, baseLLVMType);
         IRBuilder<> builder(entry);
         builder.CreateRet(val);
       }}};

  ctx->registerType(genType, llvmType, dfltBuilder, inlineMagics, nonInlineMagicFuncs);
  typeResult = llvmType;
}

void CodegenVisitor::visit(std::shared_ptr<types::IntNType> intNType) {
  auto *llvmType = IntegerType::getIntNTy(ctx->getLLVMContext(), intNType->getLen());

  auto dfltBuilder = [=](IRBuilder<> &builder) -> Value * {
    return ConstantInt::get(llvmType, 0);
  };

  Context::InlineMagicFuncs inlineMagicFuncs = {
      {"__new__[]",
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return dfltBuilder(builder);
       }},
      {"__new__[int]",
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return intNType->isSigned() ? builder.CreateSExtOrTrunc(args[0], llvmType)
                                     : builder.CreateZExtOrTrunc(args[0], llvmType);
       }},
      {getMagicSignature("__new__", {intNType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return args[0];
       }},
      {fmt::format(FMT_STRING("__new__[{}]"), intNType->oppositeSignName()),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return args[0];
       }},
      {getMagicSignature("__int__", {intNType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return intNType->isSigned()
                    ? builder.CreateSExtOrTrunc(args[0],
                                                ctx->getLLVMType(types::kIntType))
                    : builder.CreateZExtOrTrunc(args[0],
                                                ctx->getLLVMType(types::kIntType));
       }},
      {getMagicSignature("__copy__", {intNType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return args[0];
       }},
      {getMagicSignature("__hash__", {intNType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return intNType->isSigned()
                    ? builder.CreateSExtOrTrunc(args[0],
                                                seqIntLLVM(ctx->getLLVMContext()))
                    : builder.CreateZExtOrTrunc(args[0],
                                                seqIntLLVM(ctx->getLLVMContext()));
       }},
      {getMagicSignature("__bool__", {intNType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateZExt(
             builder.CreateICmpEQ(args[0], ConstantInt::get(llvmType, 0)),
             ctx->getLLVMType(types::kBoolType));
       }},
      {getMagicSignature("__pos__", {intNType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return args[0];
       }},
      {getMagicSignature("__neg__", {intNType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateNeg(args[0]);
       }},
      {getMagicSignature("__invert__", {intNType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateNot(args[0]);
       }},
      // int-int binary
      {getMagicSignature("__add__", {intNType, intNType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateAdd(args[0], args[1]);
       }},
      {getMagicSignature("__sub__", {intNType, intNType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateSub(args[0], args[1]);
       }},
      {getMagicSignature("__mul__", {intNType, intNType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateMul(args[0], args[1]);
       }},
      {getMagicSignature("__truediv__", {intNType, intNType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         auto *floatType = ctx->getLLVMType(types::kFloatType);

         args[0] = intNType->isSigned() ? builder.CreateSIToFP(args[0], floatType)
                                        : builder.CreateUIToFP(args[0], floatType);
         args[1] = intNType->isSigned() ? builder.CreateSIToFP(args[1], floatType)
                                        : builder.CreateUIToFP(args[1], floatType);
         return builder.CreateFDiv(args[0], args[1]);
       }},
      {getMagicSignature("__div__", {intNType, intNType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateSDiv(args[0], args[1]);
       }},
      {getMagicSignature("__mod__", {intNType, intNType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateSRem(args[0], args[1]);
       }},
      {getMagicSignature("__lshift__", {intNType, intNType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateShl(args[0], args[1]);
       }},
      {getMagicSignature("__rshift__", {intNType, intNType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return intNType->isSigned() ? builder.CreateAShr(args[0], args[1])
                                     : builder.CreateLShr(args[0], args[1]);
       }},
      {getMagicSignature("__eq__", {intNType, intNType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         auto *boolType = ctx->getLLVMType(types::kBoolType);
         return builder.CreateZExt(builder.CreateICmpEQ(args[0], args[1]), boolType);
       }},
      {getMagicSignature("__ne__", {intNType, intNType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         auto *boolType = ctx->getLLVMType(types::kBoolType);
         return builder.CreateZExt(builder.CreateICmpNE(args[0], args[1]), boolType);
       }},
      {getMagicSignature("__lt__", {intNType, intNType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         auto *boolType = ctx->getLLVMType(types::kBoolType);
         auto *value = intNType->isSigned() ? builder.CreateICmpSLT(args[0], args[1])
                                            : builder.CreateICmpULT(args[0], args[1]);
         return builder.CreateZExt(value, boolType);
       }},
      {getMagicSignature("__gt__", {intNType, intNType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         auto *boolType = ctx->getLLVMType(types::kBoolType);
         auto *value = intNType->isSigned() ? builder.CreateICmpSGT(args[0], args[1])
                                            : builder.CreateICmpUGT(args[0], args[1]);
         return builder.CreateZExt(value, boolType);
       }},
      {getMagicSignature("__le__", {intNType, intNType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         auto *boolType = ctx->getLLVMType(types::kBoolType);
         auto *value = intNType->isSigned() ? builder.CreateICmpSLE(args[0], args[1])
                                            : builder.CreateICmpULE(args[0], args[1]);
         return builder.CreateZExt(value, boolType);
       }},
      {getMagicSignature("__ge__", {intNType, intNType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         auto *boolType = ctx->getLLVMType(types::kBoolType);
         auto *value = intNType->isSigned() ? builder.CreateICmpSLE(args[0], args[1])
                                            : builder.CreateICmpULE(args[0], args[1]);
         return builder.CreateZExt(value, boolType);
       }},
      {getMagicSignature("__and__", {intNType, intNType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateAnd(args[0], args[1]);
       }},
      {getMagicSignature("__or__", {intNType, intNType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateOr(args[0], args[1]);
       }},
      {getMagicSignature("__xor__", {intNType, intNType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateOr(args[0], args[1]);
       }},
      {fmt::format(FMT_STRING("__pickle__[{}, Pointer[byte]]"), intNType->getName()),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         auto *gzWrite = cast<Function>(ctx->getModule()->getOrInsertFunction(
             "gzwrite", IntegerType::getInt32Ty(ctx->getLLVMContext()),
             IntegerType::getInt8PtrTy(ctx->getLLVMContext()),
             IntegerType::getInt8PtrTy(ctx->getLLVMContext()),
             IntegerType::getInt32Ty(ctx->getLLVMContext())));
         gzWrite->setDoesNotThrow();
         Value *buf = builder.CreateAlloca(llvmType);
         builder.CreateStore(args[0], buf);
         Value *ptr = builder.CreateBitCast(
             buf, IntegerType::getInt8PtrTy(ctx->getLLVMContext()));
         Value *size = ConstantExpr::getSizeOf(llvmType);
         builder.CreateCall(gzWrite, {args[1], ptr, size});
         return nullptr;
       }},
      {"__unpickle__[Pointer[byte]]",
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         auto *gzRead = cast<Function>(ctx->getModule()->getOrInsertFunction(
             "gzread", IntegerType::getInt32Ty(ctx->getLLVMContext()),
             IntegerType::getInt8PtrTy(ctx->getLLVMContext()),
             IntegerType::getInt8PtrTy(ctx->getLLVMContext()),
             IntegerType::getInt32Ty(ctx->getLLVMContext())));
         gzRead->setDoesNotThrow();
         Value *buf = builder.CreateAlloca(llvmType);
         Value *ptr = builder.CreateBitCast(
             buf, IntegerType::getInt8PtrTy(ctx->getLLVMContext()));
         Value *size = ConstantExpr::getSizeOf(llvmType);
         builder.CreateCall(gzRead, {args[0], ptr, size});
         return builder.CreateLoad(buf);
       }},
      {getMagicSignature("popcnt", {intNType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         Function *popcnt =
             Intrinsic::getDeclaration(ctx->getModule(), Intrinsic::ctpop, {llvmType});
         Value *count = builder.CreateCall(args[0]);
         return builder.CreateZExtOrTrunc(count, seqIntLLVM(ctx->getLLVMContext()));
       }},
  };
  if (intNType->getLen() <= 64) {
    inlineMagicFuncs[getMagicSignature("__str__", {intNType})] =
        [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
      auto *self = callMagic(intNType, getMagicSignature("__int__", {intNType}),
                             {args[0]}, builder);
      return callMagic(types::kIntType, "__str__[int]", {self}, builder);
    };
  }
  ctx->registerType(intNType, llvmType, dfltBuilder, inlineMagicFuncs, {});
}

Value *CodegenVisitor::alloc(std::shared_ptr<types::Type> type, Value *count,
                             IRBuilder<> &builder) {
  // TODO
  return nullptr;
}

Value *CodegenVisitor::callBuiltin(const std::string &signature,
                                   std::vector<Value *> args,
                                   llvm::IRBuilder<> &builder) {
  // TODO
  return nullptr;
}

llvm::Value *CodegenVisitor::callMagic(std::shared_ptr<types::Type> type,
                                       const std::string &signature,
                                       std::vector<llvm::Value *> args,
                                       llvm::IRBuilder<> &builder) {
  return nullptr;
}

Value *CodegenVisitor::codegenStr(Value *self, const std::string &name,
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

  Value *nameVal = ctx->getDefaultValue(types::kStringType, builder);
  nameVal = builder.CreateInsertValue(nameVal, len, 0);
  nameVal = builder.CreateInsertValue(nameVal, str, 1);

  return callBuiltin("_raw_type_str", {ptr, nameVal}, builder);
}

} // namespace codegen
} // namespace ir
} // namespace seq