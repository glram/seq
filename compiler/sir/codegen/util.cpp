#include "util.h"

#include "codegen.h"
#include "context.h"

#include "lang/seq.h"

#include "sir/module.h"
#include "sir/types/types.h"

#include "util/common.h"
#include "util/fmt/format.h"

namespace {

using namespace llvm;
using namespace seq;
using namespace seq::ir;

Function *makeCanonicalMainFunc(Function *realMain,
                                std::shared_ptr<codegen::Context> ctx,
                                std::shared_ptr<SIRModule> irModule) {
#define LLVM_I32() IntegerType::getInt32Ty(context)
  LLVMContext &context = realMain->getContext();
  Module *module = realMain->getParent();

  auto *func = cast<Function>(module->getOrInsertFunction(
      "main", LLVM_I32(), LLVM_I32(),
      PointerType::get(IntegerType::getInt8PtrTy(context), 0)));

  func->setPersonalityFn(seq::makePersonalityFunc(module));
  auto argiter = func->arg_begin();
  Value *argc = argiter++;
  Value *argv = argiter;
  argc->setName("argc");
  argv->setName("argv");

  auto *entry = llvm::BasicBlock::Create(context, "entry", func);
  auto *loop = llvm::BasicBlock::Create(context, "loop", func);

  IRBuilder<> builder(entry);
  auto *len = builder.CreateZExt(argc, seqIntLLVM(context));

  auto strReal = ctx->getTypeRealization(ir::types::kStringType);
  auto *ptr = strReal->alloc(len, builder, false);

  auto arrReal = ctx->getTypeRealization(ir::types::kStringArrayType);
  auto *arr = arrReal->makeNew({ptr, len}, builder);

  builder.CreateBr(loop);
  builder.SetInsertPoint(loop);

  auto *control = builder.CreatePHI(LLVM_I32(), 2, "i");
  auto *next = builder.CreateAdd(control, ConstantInt::get(LLVM_I32(), 1), "next");
  auto *cond = builder.CreateICmpSLT(control, argc);

  auto *body = llvm::BasicBlock::Create(context, "body", func);
  auto *branch = builder.CreateCondBr(cond, body, body); // we set false-branch below

  auto *strlenFunc = cast<Function>(module->getOrInsertFunction(
      "strlen", seqIntLLVM(context), IntegerType::getInt8PtrTy(context)));

  builder.SetInsertPoint(body);
  auto *arg = builder.CreateLoad(builder.CreateGEP(argv, control));
  auto *argLen = builder.CreateZExtOrTrunc(builder.CreateCall(strlenFunc, arg),
                                           seqIntLLVM(context));

  auto *str = strReal->makeNew({arg, argLen}, builder);
  Value *idx = builder.CreateZExt(control, seqIntLLVM(context));
  arrReal->callMagic(codegen::getMagicSignature(
                         "__setitem__", {ir::types::kStringArrayType,
                                         ir::types::kIntType, ir::types::kStringType}),
                     {arr, idx, str}, builder);
  builder.CreateBr(loop);

  control->addIncoming(ConstantInt::get(LLVM_I32(), 0), entry);
  control->addIncoming(next, body);

  auto *exit = llvm::BasicBlock::Create(context, "exit", func);
  branch->setSuccessor(1, exit);

  builder.SetInsertPoint(exit);
  auto *argVar = ctx->getVar(irModule->getArgVar());
  builder.CreateStore(arr, argVar);

  auto *initFunc =
      cast<Function>(module->getOrInsertFunction("seq_init", Type::getVoidTy(context)));
  initFunc->setCallingConv(CallingConv::C);
  builder.CreateCall(initFunc);

#if SEQ_HAS_TAPIR
  /*
   * Put the entire program in a parallel+single region
   */
  {
    getOrCreateKmpc_MicroTy(context);
    getOrCreateIdentTy(module);
    getOrCreateDefaultLocation(module);

    auto *IdentTyPtrTy = getIdentTyPointerTy();

    Type *forkParams[] = {IdentTyPtrTy, LLVM_I32(),
                          getKmpc_MicroPointerTy(module->getContext())};
    auto *forkFnTy = FunctionType::get(Type::getVoidTy(context), forkParams, true);
    auto *forkFunc =
        cast<Function>(module->getOrInsertFunction("__kmpc_fork_call", forkFnTy));

    Type *singleParams[] = {IdentTyPtrTy, LLVM_I32()};
    auto *singleFnTy = FunctionType::get(LLVM_I32(), singleParams, false);
    auto *singleFunc =
        cast<Function>(module->getOrInsertFunction("__kmpc_single", singleFnTy));

    Type *singleEndParams[] = {IdentTyPtrTy, LLVM_I32()};
    auto *singleEndFnTy =
        FunctionType::get(Type::getVoidTy(context), singleEndParams, false);
    auto *singleEndFunc =
        cast<Function>(module->getOrInsertFunction("__kmpc_end_single", singleEndFnTy));

    // make the proxy main function that will be called by __kmpc_fork_call:
    std::vector<Type *> proxyArgs = {PointerType::get(LLVM_I32(), 0),
                                     PointerType::get(LLVM_I32(), 0)};
    auto *proxyMainTy = FunctionType::get(Type::getVoidTy(context), proxyArgs, false);
    auto *proxyMain =
        cast<Function>(module->getOrInsertFunction("seq.proxy_main", proxyMainTy));
    proxyMain->setLinkage(GlobalValue::PrivateLinkage);
    proxyMain->setPersonalityFn(makePersonalityFunc(module));
    auto *proxyBlockEntry = llvm::BasicBlock::Create(context, "entry", proxyMain);
    auto *proxyBlockMain = llvm::BasicBlock::Create(context, "main", proxyMain);
    auto *proxyBlockExit = llvm::BasicBlock::Create(context, "exit", proxyMain);
    builder.SetInsertPoint(proxyBlockEntry);

    Value *tid = proxyMain->arg_begin();
    tid = builder.CreateLoad(tid);
    Value *singleCall = builder.CreateCall(singleFunc, {DefaultOpenMPLocation, tid});
    Value *shouldExit = builder.CreateICmpEQ(singleCall, builder.getInt32(0));
    builder.CreateCondBr(shouldExit, proxyBlockExit, proxyBlockMain);

    builder.SetInsertPoint(proxyBlockExit);
    builder.CreateRetVoid();

    invokeMain(realMain, proxyBlockMain);
    builder.SetInsertPoint(proxyBlockMain);
    builder.CreateCall(singleEndFunc, {DefaultOpenMPLocation, tid});
    builder.CreateRetVoid();

    // actually make the fork call:
    std::vector<Value *> forkArgs = {
        DefaultOpenMPLocation, builder.getInt32(0),
        builder.CreateBitCast(proxyMain, getKmpc_MicroPointerTy(context))};
    builder.SetInsertPoint(exit);
    builder.CreateCall(forkFunc, forkArgs);

    // finally, tell Tapir to NOT create its own parallel regions, as we've done
    // it here:
    fastOpenMP.setValue(true);
  }
#else
  invokeMain(realMain, exit);
#endif

  builder.SetInsertPoint(exit);
  builder.CreateRet(ConstantInt::get(LLVM_I32(), 0));
  return func;
#undef LLVM_I32
}
} // namespace

namespace seq {
namespace ir {
namespace codegen {

using namespace llvm;

GlobalVariable *getByteCompTable(Module *module, const std::string &name) {
  LLVMContext &context = module->getContext();
  auto *ty = IntegerType::getInt8Ty(context);
  GlobalVariable *table = module->getGlobalVariable(name);

  if (!table) {
    std::vector<Constant *> v(256, ConstantInt::get(ty, 0));

    for (auto &a : v)
      a = ConstantInt::get(ty, 'N');

    std::string from = "ACBDGHKMNSRUTWVYacbdghkmnsrutwvy.-";
    std::string to = "TGVHCDMKNSYAAWBRtgvhcdmknsyaawbr.-";

    for (unsigned i = 0; i < from.size(); i++)
      v[from[i]] = ConstantInt::get(ty, (uint64_t)to[i]);

    auto *arrTy = ArrayType::get(ty, v.size());
    table = new GlobalVariable(*module, arrTy, true, GlobalValue::PrivateLinkage,
                               ConstantArray::get(arrTy, v), name);
  }

  return table;
}

std::string getMagicSignature(const std::string &name,
                              std::vector<std::shared_ptr<types::Type>> types) {
  fmt::memory_buffer buf;
  for (auto it = types.begin(); it != types.end(); ++it) {
    fmt::format_to(buf, FMT_STRING("{}{}"), (*it)->getName(),
                   it + 1 == types.end() ? "" : ", ");
  }
  return fmt::format(FMT_STRING("{}[{}]"), name, std::string(buf.begin(), buf.end()));
}

Value *generatorDone(Value *self, llvm::BasicBlock *block) {
  Function *doneFn =
      Intrinsic::getDeclaration(block->getModule(), Intrinsic::coro_done);
  IRBuilder<> builder(block);
  return builder.CreateCall(doneFn, self);
}

void generatorResume(Value *self, llvm::BasicBlock *block, llvm::BasicBlock *normal,
                     llvm::BasicBlock *unwind) {
  Function *resFn =
      Intrinsic::getDeclaration(block->getModule(), Intrinsic::coro_resume);
  IRBuilder<> builder(block);
  if (normal || unwind)
    builder.CreateInvoke(resFn, normal, unwind, self);
  else
    builder.CreateCall(resFn, self);
}

Value *generatorPromise(Value *self, llvm::BasicBlock *block, Type *outType,
                        bool returnPtr) {
  if (!outType)
    return nullptr;

  LLVMContext &context = block->getContext();
  IRBuilder<> builder(block);

  Function *promFn =
      Intrinsic::getDeclaration(block->getModule(), Intrinsic::coro_promise);

  Value *aln = ConstantInt::get(
      IntegerType::getInt32Ty(context),
      block->getModule()->getDataLayout().getPrefTypeAlignment(outType));
  Value *from = ConstantInt::get(IntegerType::getInt1Ty(context), 0);

  Value *ptr = builder.CreateCall(promFn, {self, aln, from});
  ptr = builder.CreateBitCast(ptr, PointerType::get(outType, 0));
  return returnPtr ? ptr : builder.CreateLoad(ptr);
}

void generatorSend(Value *self, Value *val, llvm::BasicBlock *block, Type *outType) {
  Value *promisePtr = generatorPromise(self, block, outType, /*returnPtr=*/true);
  if (!promisePtr)
    throw exc::SeqException("cannot send value to void generator");
  IRBuilder<> builder(block);
  builder.CreateStore(val, promisePtr);
}

void generatorDestroy(Value *self, llvm::BasicBlock *block) {
  Function *destFn =
      Intrinsic::getDeclaration(block->getModule(), Intrinsic::coro_destroy);
  IRBuilder<> builder(block);
  builder.CreateCall(destFn, self);
}

void generatorYield(llvm::Value *val, llvm::BasicBlock *block, llvm::BasicBlock *dst,
                    llvm::Value *promise, llvm::BasicBlock *suspend,
                    llvm::BasicBlock *cleanup) {
  LLVMContext &context = block->getContext();
  auto *module = block->getModule();
  IRBuilder<> builder(block);

  if (val) {
    assert(promise);
    builder.CreateStore(val, promise);
  }

  Function *suspFn = Intrinsic::getDeclaration(module, Intrinsic::coro_suspend);
  Value *tok = ConstantTokenNone::get(context);
  Value *final = ConstantInt::get(IntegerType::getInt1Ty(context), dst == nullptr);
  Value *susp = builder.CreateCall(suspFn, {tok, final});

  if (!dst) {
    dst = llvm::BasicBlock::Create(context, "", suspend->getParent());
    builder.SetInsertPoint(dst);
    builder.CreateUnreachable();
    builder.SetInsertPoint(block);
  }

  /*
   * Can't have anything after the `ret` instruction we just added,
   * so make a new block and return that to the caller.
   */

  SwitchInst *inst = builder.CreateSwitch(susp, suspend, 2);
  inst->addCase(ConstantInt::get(IntegerType::getInt8Ty(context), 0), dst);
  inst->addCase(ConstantInt::get(IntegerType::getInt8Ty(context), 1), cleanup);
}

void generatorYieldIn(llvm::Value *ptr, llvm::BasicBlock *block, llvm::BasicBlock *dst,
                      llvm::Value *promise, llvm::BasicBlock *suspend,
                      llvm::BasicBlock *cleanup) {
  auto *newDst = llvm::BasicBlock::Create(block->getContext(), "loadPromise",
                                          suspend->getParent());
  generatorYield(nullptr, block, newDst, promise, suspend, cleanup);

  IRBuilder<> builder(newDst);
  builder.SetInsertPoint(newDst);
  builder.CreateStore(builder.CreateLoad(promise), ptr);
  builder.CreateBr(dst);
}

llvm::StructType *getTypeInfoType(LLVMContext &ctx) {
  return StructType::get(IntegerType::getInt32Ty(ctx));
}

llvm::StructType *getPadType(LLVMContext &ctx) {
  return StructType::get(IntegerType::getInt8PtrTy(ctx), IntegerType::getInt32Ty(ctx));
}

llvm::StructType *getExcType(LLVMContext &ctx) {
  return StructType::get(getTypeInfoType(ctx), IntegerType::getInt8PtrTy(ctx));
}

llvm::Module *compile(LLVMContext &context, std::shared_ptr<SIRModule> module) {
  auto *llvmModule = new Module(module->getName(), context);
  auto codegenCtx = std::make_shared<Context>(llvmModule);

  CodegenVisitor v(codegenCtx);
  module->accept(v);

  auto *realMain = llvmModule->getFunction("seq.main");
  makeCanonicalMainFunc(realMain, codegenCtx, module);
  verifyModuleFailFast(*llvmModule);
  optimizeModule(llvmModule);
  applyGCTransformations(llvmModule);
  verifyModuleFailFast(*llvmModule);
  optimizeModule(llvmModule);
  verifyModuleFailFast(*llvmModule);
#if SEQ_HAS_TAPIR
  tapir::resetOMPABI();
#endif

  return llvmModule;
}

} // namespace codegen
} // namespace ir
} // namespace seq
