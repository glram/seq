#include "util.h"

#include "context.h"

#include "sir/types/types.h"

#include "util/fmt/format.h"

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

void funcYield(CodegenFrame &meta, llvm::Value *val, llvm::BasicBlock *block,
               llvm::BasicBlock *dst) {
  if (!meta.isGenerator) {
    throw std::runtime_error("can only yield from generators");
  }
  LLVMContext &context = block->getContext();
  auto *module = block->getModule();
  IRBuilder<> builder(block);

  if (val) {
    assert(meta.promise);
    builder.CreateStore(val, meta.promise);
  }

  Function *suspFn = Intrinsic::getDeclaration(module, Intrinsic::coro_suspend);
  Value *tok = ConstantTokenNone::get(context);
  Value *final = ConstantInt::get(IntegerType::getInt1Ty(context), dst == nullptr);
  Value *susp = builder.CreateCall(suspFn, {tok, final});

  if (!dst) {
    dst = llvm::BasicBlock::Create(context, "", meta.func);
    builder.SetInsertPoint(dst);
    builder.CreateUnreachable();
    builder.SetInsertPoint(block);
  }

  /*
   * Can't have anything after the `ret` instruction we just added,
   * so make a new block and return that to the caller.
   */

  SwitchInst *inst = builder.CreateSwitch(susp, meta.suspend, 2);
  inst->addCase(ConstantInt::get(IntegerType::getInt8Ty(context), 0), dst);
  inst->addCase(ConstantInt::get(IntegerType::getInt8Ty(context), 1), meta.cleanup);
}

void funcYieldIn(CodegenFrame &meta, llvm::Value *ptr, llvm::BasicBlock *block,
                 llvm::BasicBlock *dst) {
  auto *newDst =
      llvm::BasicBlock::Create(block->getContext(), "loadPromise", meta.func);
  funcYield(meta, nullptr, block, newDst);

  IRBuilder<> builder(newDst);
  builder.SetInsertPoint(newDst);
  builder.CreateStore(builder.CreateLoad(meta.promise), ptr);
  builder.CreateBr(dst);
}

} // namespace codegen
} // namespace ir
} // namespace seq
