#include "codegen.h"

#include <algorithm>
#include <tuple>

#include "util.h"

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
using namespace llvm;
using namespace seq::ir;

std::string getLLVMFuncName(std::shared_ptr<Func> f) {
  return fmt::format(FMT_STRING("seq.{}{}"), f->getName(), f->getId());
}

/*
 * Recursively extract the function and args from a partial call.
 */
std::tuple<llvm::Value *, std::vector<Value *>>
processPartial(Value *self, const std::vector<Value *> &args,
               std::shared_ptr<types::PartialFuncType> p, IRBuilder<> b) {
  std::vector<Value *> argsFull;
  auto *func = b.CreateExtractValue(self, 0);
  auto callTypes = p->getCallTypes();

  auto curArg = 0;
  for (auto it = callTypes.begin(); it != callTypes.end(); ++it) {
    if (*it)
      argsFull.push_back(b.CreateExtractValue(self, it - callTypes.begin() + 1));
    else {
      argsFull.push_back(args[curArg++]);
    }
  }

  if (std::static_pointer_cast<types::FuncType>(p->getCallee())->isPartial()) {
    return processPartial(
        func, argsFull,
        std::static_pointer_cast<types::PartialFuncType>(p->getCallee()), b);
  }
  return {func, argsFull};
}
} // namespace

namespace seq {
namespace ir {
namespace codegen {

using namespace llvm;

void CodegenVisitor::visit(std::shared_ptr<IRModule> module) {
  ctx->pushFrame();

  transform(types::kBoolType);
  transform(types::kFloatType);
  transform(types::kIntType);
  transform(types::kByteType);
  transform(types::kBytePointerType);
  transform(types::kStringType);
  // transform(types::kSeqType);
  transform(types::kStringPointerType);
  transform(types::kStringArrayType);
  // transform(types::kNoArgVoidFuncType);

  for (auto &g : module->getGlobals()) {
    if (g->isFunc() && std::static_pointer_cast<Func>(g)->isBuiltin()) {
      auto fn = std::static_pointer_cast<Func>(g);

      auto *funcPtrType = cast<PointerType>(transform(fn->getType())->llvmType);
      auto *funcType = cast<FunctionType>(funcPtrType->getElementType());

      auto *llvmFunc =
          ctx->getModule()->getOrInsertFunction(fn->getMagicName(), funcType);
      ctx->stubBuiltin(fn, std::make_shared<BuiltinStub>(fn, cast<Function>(llvmFunc)));
    } else if (!g->isFunc()) {
      transform(g);
    }
  }

  for (auto &g : module->getGlobals())
    transform(g);

  transform(module->getBase(), "seq.main");
}

void CodegenVisitor::visit(std::shared_ptr<BasicBlock> block) {
  auto *llvmBlock = ctx->getBlock(block);
  ctx->getFrame().curBlock = llvmBlock;
  ctx->getFrame().curIRBlock = block;

  for (auto &inst : block->getInstructions())
    transform(inst);

  auto term = block->getTerminator();
  transform(term);
}

void CodegenVisitor::visit(std::shared_ptr<TryCatch> tc) {
  auto meta = ctx->getTryCatchMeta(tc);

  if (meta) {
    tcResult = meta;
    return;
  }

  meta = std::make_shared<TryCatchMetadata>();
  tcResult = meta;
  ctx->registerTryCatch(tc, meta);

  auto &context = ctx->getLLVMContext();
  auto *module = ctx->getModule();
  auto *func = ctx->getFrame().func;

  auto *unreachable = llvm::BasicBlock::Create(context, "", func);
  IRBuilder<> builder(unreachable);
  builder.CreateUnreachable();

  meta->excFlag = transform(tc->getFlagVar());
  meta->finallyBr = SwitchInst::Create(builder.getInt32(0), unreachable, 10);

  StructType *padType = getPadType(context);
  StructType *unwindType =
      StructType::get(IntegerType::getInt64Ty(context)); // header only
  StructType *excType = getExcType(context);

  meta->exceptionBlock = llvm::BasicBlock::Create(context, "exception", func);
  meta->exceptionRouteBlock =
      llvm::BasicBlock::Create(context, "exception_route", func);
  meta->finallyStart = ctx->getBlock(tc->getFinallyBlock().lock());

  auto *externalExceptionBlock =
      llvm::BasicBlock::Create(context, "external_exception", func);
  auto *unwindResumeBlock = llvm::BasicBlock::Create(context, "unwind_resume", func);

  using Path = std::vector<std::shared_ptr<TryCatch>>;
  llvm::BasicBlock *catchAll = nullptr;
  Path catchAllPath;
  Path totalPath = tc->getPath(nullptr);

  builder.SetInsertPoint(unwindResumeBlock);
  builder.CreateResume(builder.CreateLoad(ctx->getFrame().catchStore));

  for (auto &handler : tc->getCatchBlocks()) {
    meta->handlers.push_back(ctx->getBlock(handler.lock()));
  }

  std::vector<std::shared_ptr<types::Type>> catchTypesFull(tc->getCatchTypes());
  std::vector<llvm::BasicBlock *> handlersFull(meta->handlers);
  std::vector<Path> paths(catchTypesFull.size());
  std::vector<llvm::Value *> vars;

  for (auto i = 0; i < handlersFull.size(); ++i) {
    if (tc->getVar(i))
      vars.push_back(transform(tc->getVar(i)));
    vars.push_back(nullptr);
  }

  auto checkMatch = [](std::vector<std::shared_ptr<types::Type>> &all,
                       std::shared_ptr<types::Type> &t) -> bool {
    for (auto &cur : all)
      if (cur && cur->getId() == t->getId())
        return true;
    return false;
  };

  Path pathStub = {tc};
  auto cur = tc->getParent().lock();
  while (cur) {
    if (catchAll)
      break;

    auto curMeta = transform(cur);
    auto curCatchTypes = cur->getCatchTypes();

    for (auto i = 0; i < curCatchTypes.size(); ++i) {
      auto t = curCatchTypes[i];
      if (!t && !catchAll) {
        catchAll = curMeta->handlers[i];
        catchAllPath = Path(pathStub.begin(), pathStub.end());
        catchTypesFull.push_back(t);
        handlersFull.push_back(catchAll);
        paths.push_back(catchAllPath);

        if (cur->getVar(i))
          vars.push_back(transform(cur->getVar(i)));
        else
          vars.push_back(nullptr);
      } else if (!checkMatch(catchTypesFull, t)) {
        catchTypesFull.push_back(t);
        handlersFull.push_back(curMeta->handlers[i]);
        paths.emplace_back(pathStub.begin(), pathStub.end());

        if (cur->getVar(i))
          vars.push_back(transform(cur->getVar(i)));
        else
          vars.push_back(nullptr);
      }
    }

    cur = cur->getParent().lock();
  }

  builder.SetInsertPoint(meta->exceptionBlock);
  auto *caughtResult =
      builder.CreateLandingPad(padType, (unsigned)catchTypesFull.size());
  caughtResult->setCleanup(true);

  for (auto &catchType : catchTypesFull) {
    auto *tidx = new GlobalVariable(
        *module, getTypeInfoType(context), true, GlobalValue::PrivateLinkage,
        ConstantStruct::get(getTypeInfoType(context),
                            builder.getInt32(catchType->getId())));
    caughtResult->addClause(tidx);
  }

  auto *unwindException = builder.CreateExtractValue(caughtResult, 0);
  builder.CreateStore(caughtResult, ctx->getFrame().catchStore);
  auto *unwindExceptionClass = builder.CreateLoad(builder.CreateStructGEP(
      unwindType,
      builder.CreatePointerCast(unwindException, unwindType->getPointerTo()), 0));

  // Check for foreign
  builder.CreateCondBr(
      builder.CreateICmpEQ(
          unwindExceptionClass,
          ConstantInt::get(IntegerType::getInt64Ty(context), seq_exc_class())),
      meta->exceptionRouteBlock, externalExceptionBlock);

  // External exception
  builder.SetInsertPoint(externalExceptionBlock);
  builder.CreateUnreachable();

  // reroute Seq exceptions:
  builder.SetInsertPoint(meta->exceptionRouteBlock);
  unwindException =
      builder.CreateExtractValue(builder.CreateLoad(ctx->getFrame().catchStore), 0);
  auto *excVal = builder.CreatePointerCast(
      builder.CreateConstGEP1_64(unwindException, (uint64_t)seq_exc_offset()),
      excType->getPointerTo());

  auto *loadedExc = builder.CreateLoad(excVal);
  auto *objType = builder.CreateExtractValue(loadedExc, 0);
  objType = builder.CreateExtractValue(objType, 0);
  auto *objPtr = builder.CreateExtractValue(loadedExc, 1);

  llvm::BasicBlock *dfltRoute = llvm::BasicBlock::Create(context, "dflt", func);
  ctx->getFrame().curBlock = dfltRoute;
  if (catchAll)
    codegenTcJump(catchAll, catchAllPath);
  else
    codegenTcJump(unwindResumeBlock, totalPath);

  builder.SetInsertPoint(meta->exceptionRouteBlock);
  auto switchToCatch =
      builder.CreateSwitch(objType, dfltRoute, (unsigned)handlersFull.size());
  for (auto i = 0; i < handlersFull.size(); ++i) {
    auto *catchSetup =
        llvm::BasicBlock::Create(context, fmt::format(FMT_STRING("catch{}"), i), func);
    ctx->getFrame().curBlock = catchSetup;
    builder.SetInsertPoint(catchSetup);
    if (vars[i]) {
      builder.CreateStore(
          builder.CreateBitCast(objPtr, transform(catchTypesFull[i])->llvmType),
          vars[i]);
    }
    codegenTcJump(handlersFull[i], paths[i]);
    if (catchTypesFull[i]) {
      switchToCatch->addCase(builder.getInt32(catchTypesFull[i]->getId()), catchSetup);
    }
  }
}

void CodegenVisitor::visit(std::shared_ptr<Func> sirFunc) {
  auto name = nameOverride.empty() ? (sirFunc->isBuiltin() || sirFunc->isExternal()
                                          ? sirFunc->getName()
                                          : getLLVMFuncName(sirFunc))
                                   : nameOverride;

  if (auto *llvmFunc = ctx->getModule()->getFunction(name)) {
    if (!sirFunc->isBuiltin() ||
        ctx->getBuiltinStub(sirFunc->getMagicName())->doneGen) {
      varResult = llvmFunc;
      return;
    } else if (sirFunc->isBuiltin()) {
      ctx->getBuiltinStub(sirFunc->getMagicName())->doneGen = true;
    }
  }

  auto *module = ctx->getModule();
  auto isGen = sirFunc->isGenerator();

  auto sirFuncType = std::static_pointer_cast<types::FuncType>(sirFunc->getType());
  auto outTypeRealization =
      isGen ? transform(
                  std::static_pointer_cast<types::Generator>(sirFuncType->getRType())
                      ->getBase())
            : transform(sirFuncType->getRType());

  auto typeRealization = transform(sirFuncType);
  auto *funcPtrType = cast<PointerType>(typeRealization->llvmType);
  auto *funcType = cast<FunctionType>(funcPtrType->getElementType());
  auto *outLLVMType = outTypeRealization->llvmType;

  auto *func = cast<Function>(module->getOrInsertFunction(name, funcType));
  varResult = func;

  if (sirFunc->isExternal())
    return;
  else if (sirFunc->isInternal()) {
    transform(sirFunc->getParentType())
        ->getMagicBuilder(getMagicSignature(sirFunc->getMagicName(),
                                            sirFuncType->getArgTypes()))(func);
    return;
  }

  auto funcAttributes = sirFunc->getAttribute<FuncAttribute>(kFuncAttribute);
  auto srcInfoAttribute = sirFunc->getAttribute<SrcInfoAttribute>(kSrcInfoAttribute);
  auto srcInfo = srcInfoAttribute ? srcInfoAttribute->info : seq::SrcInfo();

  if (funcAttributes) {
    if (funcAttributes->has("export")) {
      if (sirFunc->getEnclosingFunc())
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
  frame.func = func;
  frame.irFunc = sirFunc;
  frame.isGenerator = isGen;
  frame.outReal = outTypeRealization;

  auto *preamble = llvm::BasicBlock::Create(context, "preamble", func);

  // Stub blocks
  for (auto &b : sirFunc->getBlocks()) {
    auto blockName = fmt::format(FMT_STRING("bb{}"), b->getId());
    ctx->registerBlock(b, llvm::BasicBlock::Create(context, blockName, func));
  }

  IRBuilder<> builder(preamble);
  frame.curBlock = preamble;

  // Register vars
  for (auto &v : sirFunc->getVars()) {
    transform(v)->setName(v->getName());
  }

  // Register arg vars
  auto argIter = func->arg_begin();
  for (auto &a : sirFunc->getArgVars()) {
    auto *var = transform(a);
    var->setName(a->getName());
    builder.SetInsertPoint(frame.curBlock);
    builder.CreateStore(argIter++, var);
  }

  auto *padType = getPadType(context);
  if (sirFunc->getTryCatch()) {
    frame.catchStore = makeAlloca(ConstantAggregateZero::get(padType), preamble);
    transform(sirFunc->getTryCatch());
  }

  // General generator primitives
  Value *id = nullptr;
  if (isGen) {
    Function *idFn = Intrinsic::getDeclaration(module, Intrinsic::coro_id);
    Value *nullPtr = ConstantPointerNull::get(IntegerType::getInt8PtrTy(context));

    if (outTypeRealization->irType->getId() != types::kVoidType->getId()) {
      auto *promise = outTypeRealization->alloc(builder, true);
      promise->setName("promise");

      frame.promise = promise;

      auto *promiseRaw =
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
  } else if (!funcType->getReturnType()->isVoidTy()) {
    builder.SetInsertPoint(frame.curBlock);
    frame.rValPtr = outTypeRealization->alloc(builder, true);
    builder.CreateStore(outTypeRealization->getDefaultValue(builder), frame.rValPtr);
  }

  llvm::BasicBlock *allocBlock = nullptr;
  Value *alloc = nullptr;
  if (isGen) {
    allocBlock = llvm::BasicBlock::Create(context, "alloc", func);
    builder.SetInsertPoint(allocBlock);
    auto *sizeFn =
        Intrinsic::getDeclaration(module, Intrinsic::coro_size, {seqIntLLVM(context)});
    auto *size = builder.CreateCall(sizeFn);
    auto *allocFunc = makeAllocFunc(module, false);
    alloc = builder.CreateCall(allocFunc, size);
  }

  auto *entry = llvm::BasicBlock::Create(context, "entry", func);
  auto *entryActual = entry;
  llvm::BasicBlock *dynFree = nullptr;

  if (isGen) {
    builder.CreateBr(entry);

    builder.SetInsertPoint(entry);

    PHINode *phi = builder.CreatePHI(IntegerType::getInt8PtrTy(context), 2);
    phi->addIncoming(ConstantPointerNull::get(IntegerType::getInt8PtrTy(context)),
                     preamble);
    phi->addIncoming(alloc, allocBlock);

    Function *beginFn = Intrinsic::getDeclaration(module, Intrinsic::coro_begin);

    auto *handle = builder.CreateCall(beginFn, {id, phi});
    handle->setName("hdl");

    frame.handle = handle;

    /*
     * Cleanup code
     */
    auto *cleanup = llvm::BasicBlock::Create(context, "cleanup", func);
    frame.cleanup = cleanup;
    dynFree = llvm::BasicBlock::Create(context, "dyn_free", func);

    builder.SetInsertPoint(cleanup);
    auto *freeFn = Intrinsic::getDeclaration(module, Intrinsic::coro_free);
    auto *mem = builder.CreateCall(freeFn, {id, handle});
    auto *needDynFree = builder.CreateIsNotNull(mem);

    auto *suspend = llvm::BasicBlock::Create(context, "suspend", func);
    frame.suspend = suspend;

    builder.CreateCondBr(needDynFree, dynFree, suspend);

    builder.SetInsertPoint(dynFree);
    builder.CreateBr(suspend);

    builder.SetInsertPoint(suspend);
    Function *endFn = Intrinsic::getDeclaration(module, Intrinsic::coro_end);
    builder.CreateCall(endFn,
                       {handle, ConstantInt::get(IntegerType::getInt1Ty(context), 0)});
    builder.CreateRet(handle);

    frame.exit = llvm::BasicBlock::Create(context, "final", func);
    funcYield(frame, nullptr, frame.exit, nullptr);
  } else {
    frame.exit = llvm::BasicBlock::Create(context, "final", func);
    builder.SetInsertPoint(frame.exit);
    if (funcType->getReturnType()->isVoidTy())
      builder.CreateRetVoid();
    else
      builder.CreateRet(builder.CreateLoad(frame.rValPtr));
  }

  if (sirFunc->isGenerator()) {
    auto *newEntry = llvm::BasicBlock::Create(context, "actualEntry", func);
    funcYield(frame, nullptr, entry, newEntry);
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

    frame.exit->moveAfter(&func->getBasicBlockList().back());
    frame.cleanup->moveAfter(frame.exit);
    dynFree->moveAfter(frame.cleanup);
    frame.suspend->moveAfter(dynFree);
  } else {
    builder.CreateBr(entry);
  }

  ctx->popFrame();
}

void CodegenVisitor::visit(std::shared_ptr<Var> var) {
  if (auto *val = ctx->getVar(var)) {
    varResult = val;
    return;
  }

  // TODO: repl!
  auto typeRealization = transform(var->getType());
  auto *llvmType = typeRealization->llvmType;
  Value *val;
  if (var->isGlobal()) {
    val = new GlobalVariable(*ctx->getModule(), llvmType, false,
                             GlobalValue::PrivateLinkage,
                             Constant::getNullValue(llvmType), var->getName());
  } else {
    IRBuilder<> builder(ctx->getFrame().curBlock);
    val = typeRealization->alloc(builder, true);
  }

  ctx->registerVar(var, val);
  varResult = val;
}

void CodegenVisitor::visit(std::shared_ptr<AssignInstr> assignInstr) {
  auto *rhs = transform(assignInstr->getRhs());
  auto *lhs = transform(assignInstr->getLhs());

  IRBuilder<> builder(ctx->getFrame().curBlock);
  instrResult = builder.CreateStore(rhs, lhs);
}

void CodegenVisitor::visit(std::shared_ptr<RvalueInstr> rvalInstr) {
  transform(rvalInstr->getRvalue());
}

void CodegenVisitor::visit(std::shared_ptr<MemberRvalue> memberRval) {
  auto var = transform(memberRval->getVar());

  IRBuilder<> builder(ctx->getFrame().curBlock);
  rvalResult = transform(memberRval->getVar()->getType())
                   ->extractMember(var, memberRval->getField(), builder);
}

void CodegenVisitor::visit(std::shared_ptr<CallRvalue> callRval) {
  auto func = transform(callRval->getFunc());

  // No need to transform, function declaration does
  auto sirFuncType =
      std::static_pointer_cast<types::FuncType>(callRval->getFunc()->getType());

  std::vector<Value *> args;

  // TODO probably no longer required
  if (sirFuncType->isPartial()) {
    IRBuilder<> builder(ctx->getFrame().curBlock);
    std::tie(func, args) = processPartial(
        func, args, std::static_pointer_cast<types::PartialFuncType>(sirFuncType),
        builder);
  } else {
    for (auto &arg : callRval->getArgs()) {
      args.push_back(transform(arg));
    }
  }

  auto tc = ctx->getFrame().curIRBlock->getHandlerTryCatch();

  IRBuilder<> builder(ctx->getFrame().curBlock);
  if (tc) {
    auto tcMeta = transform(tc);

    auto &context = ctx->getLLVMContext();
    auto *unwind = tcMeta->exceptionBlock;
    auto *parent = ctx->getFrame().func;
    auto *normal = llvm::BasicBlock::Create(context, "normal", parent);
    ctx->getFrame().curBlock = normal;

    rvalResult = builder.CreateInvoke(func, normal, unwind, args);
  } else {
    rvalResult = builder.CreateCall(func, args);
  }
}

void CodegenVisitor::visit(std::shared_ptr<PartialCallRvalue> partial) {
  auto typeRealization = transform(partial->getType());

  auto *val = typeRealization->getUndefValue();
  auto *func = transform(partial->getFunc());

  IRBuilder<> builder(ctx->getFrame().curBlock);
  builder.CreateInsertValue(val, func, 0);

  auto partialArgs = partial->getArgs();
  for (auto it = partialArgs.begin(); it != partialArgs.end(); ++it) {
    if (*it) {
      auto *res = transform(*it);
      builder.SetInsertPoint(ctx->getFrame().curBlock);
      builder.CreateInsertValue(val, res, it - partialArgs.begin() + 1);
    }
  }

  rvalResult = val;
}

void CodegenVisitor::visit(std::shared_ptr<OperandRvalue> opRval) {
  rvalResult = transform(opRval->getOperand());
}

void CodegenVisitor::visit(std::shared_ptr<MatchRvalue> matchRval) {
  auto *op = transform(matchRval->getOperand());
  auto boolReal = transform(types::kBoolType);

  auto patResult = transform(matchRval->getPattern())(op);
  IRBuilder<> builder(ctx->getFrame().curBlock);
  rvalResult = builder.CreateZExt(patResult, boolReal->llvmType);
}

void CodegenVisitor::visit(std::shared_ptr<PipelineRvalue> node) {
  // TODO!
  assert(false);
}

void CodegenVisitor::visit(std::shared_ptr<StackAllocRvalue> stackAllocRval) {
  auto arrIRType = std::static_pointer_cast<types::Array>(stackAllocRval->getType());

  auto arrTypeRealization = transform(arrIRType);
  auto baseTypeRealization = transform(arrIRType->getBase());

  IRBuilder<> builder(ctx->getFrame().curBlock);

  auto *len =
      ConstantInt::get(seqIntLLVM(ctx->getLLVMContext()), stackAllocRval->getCount());
  auto *ptr = baseTypeRealization->alloc(len, builder, true);

  rvalResult = arrTypeRealization->makeNew({ptr, len}, builder);
}

void CodegenVisitor::visit(std::shared_ptr<VarLvalue> varLvalue) {
  lvalResult = transform(varLvalue->getVar());
}

void CodegenVisitor::visit(std::shared_ptr<VarMemberLvalue> varMemberLvalue) {
  auto typeRealization = transform(varMemberLvalue->getVar()->getType());
  auto var = transform(varMemberLvalue->getVar());

  IRBuilder<> builder(ctx->getFrame().curBlock);
  lvalResult =
      typeRealization->getMemberPointer(var, varMemberLvalue->getField(), builder);
}

void CodegenVisitor::visit(std::shared_ptr<VarOperand> varOperand) {
  opResult = transform(varOperand->getVar());

  if (!varOperand->getVar()->isFunc()) {
    auto opType = transform(varOperand->getType());

    IRBuilder<> builder(ctx->getFrame().curBlock);
    opResult = opType->load(opResult, builder);
  }
}

void CodegenVisitor::visit(std::shared_ptr<VarPointerOperand> varPtrOperand) {
  opResult = transform(varPtrOperand->getVar());
}

void CodegenVisitor::visit(std::shared_ptr<LiteralOperand> literalOperand) {
  auto typeRealization = transform(literalOperand->getType());
  auto *literalType = typeRealization->llvmType;

  auto contentType = literalOperand->getLiteralType();

  // TODO seq/kmer?
  if (contentType == STR) {
    IRBuilder<> builder(ctx->getFrame().curBlock);

    auto &context = ctx->getLLVMContext();
    auto *module = ctx->getModule();
    auto string = literalOperand->getSval();

    auto *strVar = new GlobalVariable(
        *module,
        llvm::ArrayType::get(IntegerType::getInt8Ty(context), string.length() + 1),
        true, GlobalValue::PrivateLinkage,
        ConstantDataArray::getString(context, string), "sval");
    strVar->setAlignment(1);

    opResult = typeRealization->makeNew(
        {builder.CreateBitCast(strVar, IntegerType::getInt8PtrTy(context)),
         ConstantInt::get(seqIntLLVM(context), string.length())},
        builder);

  } else if (contentType == INT) {
    opResult = ConstantInt::get(literalType, literalOperand->getIval());
  } else if (contentType == FLOAT) {
    opResult = ConstantFP::get(literalType, literalOperand->getFval());
  } else if (contentType == BOOL) {
    opResult = ConstantInt::get(literalType, literalOperand->getBval());
  } else {
    assert(false);
  }
}

void CodegenVisitor::visit(std::shared_ptr<common::LLVMOperand> llvmOperand) {
  opResult = llvmOperand->getValue();
}

void CodegenVisitor::visit(std::shared_ptr<WildcardPattern> wildcardPattern) {
  auto *var = transform(wildcardPattern->getVar());

  auto codegenCtx = ctx;
  patResult = [=](llvm::Value *val) -> llvm::Value * {
    IRBuilder<> builder(codegenCtx->getFrame().curBlock);
    builder.CreateStore(val, var);
    return ConstantInt::get(IntegerType::getInt1Ty(codegenCtx->getLLVMContext()), 1);
  };
}

void CodegenVisitor::visit(std::shared_ptr<BoundPattern> boundPattern) {
  auto *var = transform(boundPattern->getVar());

  auto codegenCtx = ctx;
  patResult = [=](llvm::Value *val) -> llvm::Value * {
    IRBuilder<> builder(codegenCtx->getFrame().curBlock);

    builder.CreateStore(val, var);
    return ConstantInt::get(IntegerType::getInt1Ty(codegenCtx->getLLVMContext()), 1);
  };
}

void CodegenVisitor::visit(std::shared_ptr<StarPattern>) {
  // TODO
  assert(false);
}

void CodegenVisitor::visit(std::shared_ptr<IntPattern> intPattern) {
  auto *intType = transform(types::kIntType)->llvmType;

  auto codegenCtx = ctx;
  patResult = [=](llvm::Value *val) -> llvm::Value * {
    IRBuilder<> builder(codegenCtx->getFrame().curBlock);

    Value *pat = ConstantInt::get(intType, intPattern->getValue());
    return builder.CreateICmpEQ(val, pat);
  };
}

void CodegenVisitor::visit(std::shared_ptr<BoolPattern> boolPattern) {
  auto *boolType = transform(types::kBoolType)->llvmType;

  auto codegenCtx = ctx;
  patResult = [=](llvm::Value *val) -> llvm::Value * {
    IRBuilder<> builder(codegenCtx->getFrame().curBlock);

    Value *pat = ConstantInt::get(boolType, (uint64_t)boolPattern->getValue());
    return builder.CreateICmpEQ(val, pat);
  };
}

void CodegenVisitor::visit(std::shared_ptr<StrPattern> strPattern) {
  auto typeRealization = transform(types::kStringType);

  auto codegenCtx = ctx;
  patResult = [=](llvm::Value *val) -> llvm::Value * {
    IRBuilder<> builder(codegenCtx->getFrame().curBlock);

    auto &context = codegenCtx->getLLVMContext();
    auto *module = codegenCtx->getModule();
    auto string = strPattern->getValue();

    auto *strVar = new GlobalVariable(
        *module,
        llvm::ArrayType::get(IntegerType::getInt8Ty(context), string.length() + 1),
        true, GlobalValue::PrivateLinkage,
        ConstantDataArray::getString(context, string), "sval");
    strVar->setAlignment(1);

    auto *len = ConstantInt::get(seqIntLLVM(context), string.length());

    auto *compStr = typeRealization->makeNew(
        {builder.CreateBitCast(strVar, IntegerType::getInt8PtrTy(context)), len},
        builder);
    return builder.CreateTrunc(
        builder.CreateCall(codegenCtx->getBuiltinStub("_str_eq")->func, {val, compStr}),
        builder.getInt1Ty());
  };
}

void CodegenVisitor::visit(std::shared_ptr<SeqPattern> node) {
  // TODO
  assert(false);
}

void CodegenVisitor::visit(std::shared_ptr<RecordPattern> recordPattern) {
  auto recordType = recordPattern->getType();
  auto recordRealization = transform(recordType);

  auto codegenCtx = ctx;
  patResult = [=](llvm::Value *val) -> llvm::Value * {
    IRBuilder<> builder(codegenCtx->getFrame().curBlock);

    auto &context = codegenCtx->getLLVMContext();
    Value *result = ConstantInt::get(IntegerType::getInt1Ty(context), 1);

    std::vector<std::string> fields(recordRealization->fields.size());
    for (auto &tup : recordRealization->fields) {
      fields[tup.second] = tup.first;
    }

    auto i = 0;
    for (auto &pat : recordPattern->getPatterns()) {
      auto *mem = recordRealization->extractMember(val, fields[i++], builder);
      auto *patResult = transform(pat)(mem);
      builder.SetInsertPoint(codegenCtx->getFrame().curBlock);
      result = builder.CreateAnd(result, patResult);
    }
    return result;
  };
}

void CodegenVisitor::visit(std::shared_ptr<ArrayPattern> arrayPattern) {
  auto arrayType = std::static_pointer_cast<types::Array>(arrayPattern->getType());
  auto arrTypeRealization = transform(arrayPattern->getType());
  auto baseTypeRealization = transform(arrayType->getBase());

  auto codegenCtx = ctx;
  patResult = [=](llvm::Value *val) -> llvm::Value * {
    IRBuilder<> builder(codegenCtx->getFrame().curBlock);

    auto &context = codegenCtx->getLLVMContext();
    auto *base = builder.GetInsertBlock()->getParent();

    auto hasStar = false;
    auto star = 0;

    auto patterns = arrayPattern->getPatterns();

    for (auto i = 0; i < patterns.size(); i++) {
      if (std::dynamic_pointer_cast<StarPattern>(patterns[i])) {
        if (hasStar) {
          throw std::runtime_error("can have at most one ... in list pattern.");
        }
        star = i;
        hasStar = true;
      }
    }

    auto *len = arrTypeRealization->extractMember(val, "len", builder);

    auto *lenMatch = llvm::BasicBlock::Create(context, "lenMatch", base);
    auto *patternDone = llvm::BasicBlock::Create(context, "patternDone", base);

    Value *lenOk;
    if (hasStar) {
      auto *minLen = ConstantInt::get(seqIntLLVM(context), patterns.size() - 1);
      lenOk = builder.CreateICmpSGE(len, minLen);
    } else {
      auto *expectedLen = ConstantInt::get(seqIntLLVM(context), patterns.size());
      lenOk = builder.CreateICmpEQ(len, expectedLen);
    }

    auto *start = builder.GetInsertBlock();
    builder.CreateCondBr(lenOk, lenMatch, patternDone);

    codegenCtx->getFrame().curBlock = lenMatch;
    builder.SetInsertPoint(lenMatch);

    Value *checkResult = ConstantInt::get(IntegerType::getInt1Ty(context), 1);

    auto getItem = getMagicSignature("__getitem__", {arrayType, types::kIntType});
    if (hasStar) {
      for (unsigned i = 0; i < star; i++) {
        Value *idx = ConstantInt::get(seqIntLLVM(context), i);
        auto *sub = arrTypeRealization->callMagic(getItem, {val, idx}, builder);
        auto *subRes = transform(patterns[i])(sub);
        builder.SetInsertPoint(
            codegenCtx->getFrame()
                .curBlock); // recall that pattern codegen can change the block
        checkResult = builder.CreateAnd(checkResult, subRes);
      }

      for (unsigned i = star + 1; i < patterns.size(); i++) {
        Value *idx = ConstantInt::get(seqIntLLVM(context), i);
        idx = builder.CreateAdd(idx, len);
        idx = builder.CreateSub(idx,
                                ConstantInt::get(seqIntLLVM(context), patterns.size()));
        auto *sub = arrTypeRealization->callMagic(getItem, {val, idx}, builder);
        auto *subRes = transform(patterns[i])(sub);
        builder.SetInsertPoint(
            codegenCtx->getFrame()
                .curBlock); // recall that pattern codegen can change the block
        checkResult = builder.CreateAnd(checkResult, subRes);
      }
    } else {
      for (unsigned i = 0; i < patterns.size(); i++) {
        Value *idx = ConstantInt::get(seqIntLLVM(context), i);
        auto *sub = arrTypeRealization->callMagic(getItem, {val, idx}, builder);
        auto *subRes = transform(patterns[i])(sub);
        builder.SetInsertPoint(
            codegenCtx->getFrame()
                .curBlock); // recall that pattern codegen can change the block
        checkResult = builder.CreateAnd(checkResult, subRes);
      }
    }
    auto finalBlock = llvm::BasicBlock::Create(context, "lastCheck", base);
    builder.CreateBr(finalBlock);

    builder.SetInsertPoint(finalBlock);
    builder.CreateBr(patternDone);

    codegenCtx->getFrame().curBlock = patternDone;
    builder.SetInsertPoint(patternDone);

    auto *lenMatchResult = builder.CreatePHI(IntegerType::getInt1Ty(context), 2);
    lenMatchResult->addIncoming(ConstantInt::get(IntegerType::getInt1Ty(context), 0),
                                start); // length didn't match
    lenMatchResult->addIncoming(checkResult,
                                finalBlock); // result of checking array elements  };
    return builder.CreateAnd(lenMatchResult, checkResult);
  };
}

void CodegenVisitor::visit(std::shared_ptr<RangePattern> rangePattern) {
  auto codegenCtx = ctx;

  patResult = [=](llvm::Value *val) -> llvm::Value * {
    IRBuilder<> builder(codegenCtx->getFrame().curBlock);

    auto &context = codegenCtx->getLLVMContext();
    auto *a =
        ConstantInt::get(seqIntLLVM(context), (uint64_t)rangePattern->getLower(), true);
    auto *b = ConstantInt::get(seqIntLLVM(context), (uint64_t)rangePattern->getHigher(),
                               true);
    auto *c1 = builder.CreateICmpSLE(a, val);
    auto *c2 = builder.CreateICmpSLE(val, b);
    return builder.CreateAnd(c1, c2);
  };
}

void CodegenVisitor::visit(std::shared_ptr<OrPattern> orPattern) {
  auto patterns = orPattern->getPatterns();
  auto codegenCtx = ctx;
  patResult = [=](llvm::Value *val) -> llvm::Value * {
    IRBuilder<> builder(codegenCtx->getFrame().curBlock);

    auto &context = codegenCtx->getLLVMContext();
    Value *result = ConstantInt::get(IntegerType::getInt1Ty(context), 0);

    for (auto &pattern : patterns) {
      auto *subRes = transform(pattern)(val);
      builder.SetInsertPoint(codegenCtx->getFrame().curBlock);
      result = builder.CreateOr(result, subRes);
    }

    return result;
  };
}

void CodegenVisitor::visit(std::shared_ptr<GuardedPattern> guardedPattern) {
  auto pattern = guardedPattern->getPattern();
  auto op = guardedPattern->getOperand();
  auto codegenCtx = ctx;
  patResult = [=](llvm::Value *val) -> llvm::Value * {
    LLVMContext &context = codegenCtx->getLLVMContext();

    auto *patternResult = transform(pattern)(val);
    auto *startBlock = codegenCtx->getFrame().curBlock;
    auto *block = startBlock;

    IRBuilder<> builder(block);
    block = llvm::BasicBlock::Create(context, "",
                                     block->getParent()); // guard eval block
    auto *branch = builder.CreateCondBr(patternResult, block, block);

    codegenCtx->getFrame().curBlock = block;
    auto *guardResult = transform(op);

    llvm::BasicBlock *checkBlock = codegenCtx->getFrame().curBlock;
    builder.SetInsertPoint(checkBlock);
    guardResult = builder.CreateTrunc(guardResult, IntegerType::getInt1Ty(context));

    block = llvm::BasicBlock::Create(context, "",
                                     block->getParent()); // final result block
    builder.CreateBr(block);
    branch->setSuccessor(1, block);

    builder.SetInsertPoint(block);
    auto *resultFinal = builder.CreatePHI(IntegerType::getInt1Ty(context), 2);
    resultFinal->addIncoming(ConstantInt::get(IntegerType::getInt1Ty(context), 0),
                             startBlock);              // pattern didn't match
    resultFinal->addIncoming(guardResult, checkBlock); // result of guard

    codegenCtx->getFrame().curBlock = block;
    return resultFinal;
  };
}

void CodegenVisitor::visit(std::shared_ptr<JumpTerminator> jumpTerminator) {
  auto dstIRBlock = jumpTerminator->getDst().lock();
  auto curTc = ctx->getFrame().curIRBlock->getFinallyTryCatch();
  auto tcPath = curTc ? curTc->getPath(dstIRBlock->getFinallyTryCatch())
                      : std::vector<std::shared_ptr<TryCatch>>();
  auto *dst = ctx->getBlock(dstIRBlock);
  codegenTcJump(dst, tcPath);
}

void CodegenVisitor::visit(std::shared_ptr<CondJumpTerminator> condJumpTerminator) {
  auto tDstIRBlock = condJumpTerminator->getTDst().lock();
  auto fDstIRBlock = condJumpTerminator->getFDst().lock();
  auto curTc = ctx->getFrame().curIRBlock->getFinallyTryCatch();

  auto tTcPath = curTc ? curTc->getPath(tDstIRBlock->getFinallyTryCatch())
                       : std::vector<std::shared_ptr<TryCatch>>();
  auto fTcPath = curTc ? curTc->getPath(fDstIRBlock->getFinallyTryCatch())
                       : std::vector<std::shared_ptr<TryCatch>>();

  auto *tDst = ctx->getBlock(tDstIRBlock);
  auto *fDst = ctx->getBlock(fDstIRBlock);

  auto *opVal = transform(condJumpTerminator->getCond());

  IRBuilder<> builder(ctx->getFrame().curBlock);
  opVal = builder.CreateTrunc(opVal, builder.getInt1Ty());

  if (tTcPath.empty() && fTcPath.empty()) {
    builder.CreateCondBr(opVal, tDst, fDst);
  } else {
    auto &context = ctx->getLLVMContext();
    auto *tBlock = llvm::BasicBlock::Create(context, "tDst", ctx->getFrame().func);
    auto *fBlock = llvm::BasicBlock::Create(context, "fDst", ctx->getFrame().func);

    builder.CreateCondBr(opVal, tBlock, fBlock);

    ctx->getFrame().curBlock = tBlock;
    codegenTcJump(tDst, tTcPath);

    ctx->getFrame().curBlock = fBlock;
    codegenTcJump(fDst, fTcPath);
  }
}

void CodegenVisitor::visit(std::shared_ptr<ReturnTerminator> returnTerminator) {
  auto curTc = ctx->getFrame().curIRBlock->getFinallyTryCatch();
  auto tcPath =
      curTc ? curTc->getPath(nullptr) : std::vector<std::shared_ptr<TryCatch>>();
  if (tcPath.empty()) {
    if (ctx->getFrame().isGenerator) {
      IRBuilder<> builder(ctx->getFrame().curBlock);
      builder.CreateBr(ctx->getFrame().exit);
    } else if (returnTerminator->getOperand()) {
      auto *opVal = transform(returnTerminator->getOperand());

      IRBuilder<> builder(ctx->getFrame().curBlock);
      builder.CreateRet(opVal);
    } else if (ctx->getFrame().func->getReturnType()->isVoidTy()) {
      IRBuilder<> builder(ctx->getFrame().curBlock);
      builder.CreateRetVoid();
    } else {
      IRBuilder<> builder(ctx->getFrame().curBlock);
      builder.CreateRet(ctx->getFrame().outReal->getDefaultValue(builder));
    }
    return;
  }

  if (!ctx->getFrame().isGenerator && returnTerminator->getOperand()) {
    auto *opVal = transform(returnTerminator->getOperand());

    IRBuilder<> builder(ctx->getFrame().curBlock);
    builder.CreateStore(opVal, ctx->getFrame().rValPtr);
  }

  codegenTcJump(ctx->getFrame().exit, tcPath);
}

void CodegenVisitor::visit(std::shared_ptr<YieldTerminator> yieldTerminator) {
  auto dstIRBlock = yieldTerminator->getDst().lock();
  auto curTc = ctx->getFrame().curIRBlock->getFinallyTryCatch();
  auto tcPath = curTc ? curTc->getPath(dstIRBlock->getFinallyTryCatch())
                      : std::vector<std::shared_ptr<TryCatch>>();
  auto *dst = ctx->getBlock(dstIRBlock);

  assert(tcPath.empty());

  if (yieldTerminator->getInVar()) {
    auto *ptr = transform(yieldTerminator->getInVar());
    funcYieldIn(ctx->getFrame(), ptr, ctx->getFrame().curBlock, dst);
  } else {
    auto *op = transform(yieldTerminator->getResult());
    funcYield(ctx->getFrame(), op, ctx->getFrame().curBlock, dst);
  }
}

void CodegenVisitor::visit(std::shared_ptr<ThrowTerminator> throwTerminator) {
  static auto excHeaderType = std::make_shared<types::RecordType>(
      "ExcHeader", std::vector<std::shared_ptr<types::Type>>{
                       types::kStringType, types::kStringType, types::kStringType,
                       types::kStringType, types::kIntType, types::kIntType});

  auto excHeaderRealization = transform(excHeaderType);

  if (!throwTerminator->getOperand()->getType()->isRef())
    throw std::runtime_error("cannot throw non-reference type");

  auto excIRType = throwTerminator->getOperand()->getType();
  auto *op = transform(throwTerminator->getOperand());

  auto srcInfoAttr = std::static_pointer_cast<SrcInfoAttribute>(
      throwTerminator->getAttribute(kSrcInfoAttribute));
  auto srcInfo = srcInfoAttr ? srcInfoAttr->info : SrcInfo();

  LLVMContext &context = ctx->getLLVMContext();
  Module *module = ctx->getModule();
  Function *excAllocFunc = makeExcAllocFunc(module);
  Function *throwFunc = makeThrowFunc(module);

  IRBuilder<> builder(ctx->getFrame().curBlock);
  auto *hdrType = excHeaderRealization->llvmType;
  auto *hdrPtr = builder.CreateBitCast(op, hdrType->getPointerTo());

  auto funcNameStr = ctx->getFrame().func->getName();
  auto fileNameStr = srcInfo.file;
  auto fileLine = srcInfo.line;
  auto fileCol = srcInfo.col;

  auto *funcNameVal = transform(std::make_shared<LiteralOperand>(funcNameStr));
  auto *fileNameVal = transform(std::make_shared<LiteralOperand>(fileNameStr));

  builder.SetInsertPoint(ctx->getFrame().curBlock);
  auto *fileLineVal = ConstantInt::get(seqIntLLVM(context), fileLine, true);
  auto *fileColVal = ConstantInt::get(seqIntLLVM(context), fileCol, true);

  builder.CreateStore(funcNameVal,
                      excHeaderRealization->getMemberPointer(hdrPtr, "3", builder));
  builder.CreateStore(fileNameVal,
                      excHeaderRealization->getMemberPointer(hdrPtr, "4", builder));
  builder.CreateStore(fileLineVal,
                      excHeaderRealization->getMemberPointer(hdrPtr, "5", builder));
  builder.CreateStore(fileColVal,
                      excHeaderRealization->getMemberPointer(hdrPtr, "6", builder));

  auto *exc = builder.CreateCall(excAllocFunc,
                                 {ConstantInt::get(IntegerType::getInt32Ty(context),
                                                   (uint64_t)excIRType->getId(), true),
                                  op});

  auto tc = ctx->getFrame().curIRBlock->getHandlerTryCatch();
  if (tc) {
    auto *unwind = ctx->getFrame().tryCatchMeta[tc->getId()]->exceptionBlock;
    auto *parent = ctx->getFrame().func;
    auto *normal = llvm::BasicBlock::Create(context, "normal", parent);
    ctx->getFrame().curBlock = normal;

    rvalResult = builder.CreateInvoke(throwFunc, normal, unwind, exc);
    builder.SetInsertPoint(normal);
    builder.CreateUnreachable();
  } else {
    rvalResult = builder.CreateCall(throwFunc, exc);
    builder.CreateUnreachable();
  }
}

void CodegenVisitor::visit(std::shared_ptr<AssertTerminator> assertTerminator) {
  auto dstIRBlock = assertTerminator->getDst().lock();
  auto curTc = ctx->getFrame().curIRBlock->getFinallyTryCatch();
  auto tcPath = curTc ? curTc->getPath(dstIRBlock->getFinallyTryCatch())
                      : std::vector<std::shared_ptr<TryCatch>>();
  auto *dst = ctx->getBlock(dstIRBlock);

  assert(tcPath.empty());

  auto &context = ctx->getLLVMContext();
  auto *module = ctx->getModule();
  auto *func = ctx->getFrame().func;
  auto funcAttr =
      ctx->getFrame().curIRBlock->getAttribute<FuncAttribute>(kFuncAttribute);

  const auto test = funcAttr && funcAttr->has("test");

  auto *check = transform(assertTerminator->getOperand());
  auto *msgVal =
      transform(std::make_shared<LiteralOperand>(assertTerminator->getMsg()));

  auto *fail = llvm::BasicBlock::Create(context, "assert_fail", func);

  IRBuilder<> builder(ctx->getFrame().curBlock);
  check = builder.CreateTrunc(check, builder.getInt1Ty());
  builder.CreateCondBr(check, dst, fail);

  ctx->getFrame().curBlock = fail;
  builder.SetInsertPoint(fail);

  if (test) {
    auto srcInfoAttr = std::static_pointer_cast<SrcInfoAttribute>(
        assertTerminator->getAttribute(kSrcInfoAttribute));
    auto srcInfo = srcInfoAttr ? srcInfoAttr->info : SrcInfo();

    auto *file = transform(std::make_shared<LiteralOperand>(srcInfo.file));
    auto *line = ConstantInt::get(seqIntLLVM(context), srcInfo.line);
    builder.SetInsertPoint(ctx->getFrame().curBlock);

    auto *failed = ctx->getBuiltinStub("_test_failed")->func;
    builder.CreateCall(failed, {file, line, msgVal});
  } else {
    auto builtin = ctx->getBuiltinStub("_make_assert_error");
    auto *excVal = builder.CreateCall(builtin->func, msgVal);

    auto excType =
        std::static_pointer_cast<types::FuncType>(builtin->sirFunc->getType())
            ->getRType();
    auto exc = std::make_shared<common::LLVMOperand>(excType, excVal);

    auto term = std::make_shared<ThrowTerminator>(exc);
    auto srcInfoAttr = term->getAttribute<SrcInfoAttribute>(kSrcInfoAttribute);
    auto newSrcInfo =
        std::make_shared<SrcInfoAttribute>(srcInfoAttr ? srcInfoAttr->info : SrcInfo());
    term->setAttribute(kSrcInfoAttribute, newSrcInfo);
    transform(term);
  }
}

void CodegenVisitor::visit(std::shared_ptr<FinallyTerminator> finallyTerminator) {
  auto meta = transform(finallyTerminator->getTryCatch());
  IRBuilder<> builder(ctx->getFrame().curBlock);

  meta->finallyBr->setCondition(builder.CreateLoad(meta->excFlag));
  ctx->getFrame().curBlock->getInstList().push_back(meta->finallyBr);
}

void CodegenVisitor::visit(std::shared_ptr<types::Type> type) {
  if (auto real = ctx->getTypeRealization(type)) {
    typeResult = real;
    return;
  }
  throw std::runtime_error("cannot codegen non-derived type.");
}

void CodegenVisitor::visit(std::shared_ptr<types::RecordType> memberedType) {
  if (auto real = ctx->getTypeRealization(memberedType)) {
    typeResult = real;
    return;
  }

  std::vector<Type *> body;
  std::vector<std::shared_ptr<TypeRealization>> bodyRealizations;
  for (auto &bodyType : memberedType->getMemberTypes()) {
    auto realization = transform(bodyType);
    bodyRealizations.push_back(realization);
    body.push_back(realization->llvmType);
  }
  auto *llvmType = StructType::get(ctx->getLLVMContext(), body);
  llvmType->setName(memberedType->getName());

  auto dfltBuilder = [bodyRealizations, llvmType](IRBuilder<> &builder) -> Value * {
    Value *self = UndefValue::get(llvmType);
    auto i = 0;
    for (auto &bReal : bodyRealizations) {
      if (bReal)
        self = builder.CreateInsertValue(self, bReal->getDefaultValue(builder), i++);
      else
        ++i;
    }
    return self;
  };

  auto newSig = getMagicSignature("__new__", memberedType->getMemberTypes());
  TypeRealization::InlineMagics inlineMagics = {
      {newSig,
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         Value *val = UndefValue::get(llvmType);
         for (auto it = args.begin(); it != args.end(); ++it)
           val = builder.CreateInsertValue(val, *it, it - args.begin());
         return val;
       }},
  };
  TypeRealization::NonInlineMagics nonInlineMagicFuncs = {};

  auto heterogeneous = memberedType->getMemberTypes().empty();
  if (!heterogeneous) {
    for (auto &typ : memberedType->getMemberTypes()) {
      heterogeneous =
          heterogeneous || typ->getId() != memberedType->getMemberTypes()[0]->getId();
    }
  }

  auto codegenCtx = ctx;
  if (!heterogeneous) {
    nonInlineMagicFuncs[getMagicSignature(
        "__getitem__", {memberedType, types::kIntType})] = [=](Function *func) {
      func->setLinkage(GlobalValue::PrivateLinkage);
      llvm::Type *baseType = body[0];

      auto iter = func->arg_begin();
      Value *self = iter++;
      Value *idx = iter;
      llvm::BasicBlock *entry =
          llvm::BasicBlock::Create(codegenCtx->getLLVMContext(), "entry", func);

      IRBuilder<> b(entry);
      b.SetInsertPoint(entry);
      Value *ptr = b.CreateAlloca(llvmType);
      b.CreateStore(self, ptr);
      ptr = b.CreateBitCast(ptr, baseType->getPointerTo());
      ptr = b.CreateGEP(ptr, idx);
      b.CreateRet(b.CreateLoad(ptr));
    };
  }

  TypeRealization::Fields fields;

  auto fieldIndex = 0;
  for (auto &name : memberedType->getMemberNames()) {
    fields[name] = fieldIndex++;
  }

  auto typeRealization = std::make_shared<TypeRealization>(
      memberedType, llvmType, dfltBuilder, inlineMagics, newSig, nonInlineMagicFuncs,
      fields);
  ctx->registerType(memberedType, typeRealization);
  typeResult = typeRealization;
}

void CodegenVisitor::visit(std::shared_ptr<types::RefType> refType) {
  if (auto real = ctx->getTypeRealization(refType)) {
    typeResult = real;
    return;
  }

  auto *llvmType = IntegerType::getInt8PtrTy(ctx->getLLVMContext());

  auto contents = refType->getContents();
  auto contentsRealization = transform(contents);

  auto dfltBuilder = [=](IRBuilder<> &builder) -> Value * {
    return ConstantPointerNull::get(llvmType);
  };

  auto newSig = getMagicSignature("__new__", {});

  TypeRealization::InlineMagics inlineMagics = {
      {newSig,
       [contentsRealization, refType, llvmType](std::vector<Value *>,
                                                IRBuilder<> &builder) -> Value * {
         assert(refType->getContents());
         auto *self = contentsRealization->alloc(builder, false);
         self = builder.CreateBitCast(self, llvmType);
         return self;
       }},
      {getMagicSignature("__raw__", {refType}),
       [](std::vector<Value *> args, IRBuilder<> &) -> Value * { return args[0]; }},
  };

  TypeRealization::Fields fields;
  TypeRealization::CustomGetters customGetters;

  auto fieldIndex = 0;
  for (auto &name : refType->getMemberNames()) {
    fields[name] = fieldIndex;
    customGetters[name] = [fieldIndex,
                           contentsRealization](llvm::Value *refVal,
                                                llvm::IRBuilder<> &builder) -> Value * {
      refVal =
          builder.CreateBitCast(refVal, contentsRealization->llvmType->getPointerTo());
      return builder.CreateExtractValue(builder.CreateLoad(refVal), fieldIndex);
    };
    ++fieldIndex;
  }

  TypeRealization::MemberPointerFunc memberPointerFunc =
      [contentsRealization](llvm::Value *ptr, int field,
                            llvm::IRBuilder<> &builder) -> llvm::Value * {
    auto *ptrTy = contentsRealization->llvmType->getPointerTo();
    ptr = builder.CreateBitCast(ptr, ptrTy->getPointerTo());
    auto *zeroIndex = builder.getInt32(0);
    return builder.CreateInBoundsGEP(builder.CreateLoad(ptr),
                                     {zeroIndex, builder.getInt32(field)});
  };

  auto typeRealization = std::make_shared<TypeRealization>(
      refType, llvmType, dfltBuilder, inlineMagics, newSig,
      TypeRealization::NonInlineMagics(), fields, customGetters, memberPointerFunc);
  ctx->registerType(refType, typeRealization);
  typeResult = typeRealization;
}

void CodegenVisitor::visit(std::shared_ptr<types::FuncType> funcType) {
  if (auto real = ctx->getTypeRealization(funcType)) {
    typeResult = real;
    return;
  }

  auto *rType = transform(funcType->getRType())->llvmType;

  std::vector<Type *> args;
  for (auto argType : funcType->getArgTypes()) {
    args.push_back(transform(argType)->llvmType);
  }

  auto *llvmType = PointerType::get(FunctionType::get(rType, args, false), 0);

  auto dfltBuilder = [=](IRBuilder<> &builder) -> Value * {
    return ConstantPointerNull::get(llvmType);
  };

  auto callArgs = funcType->getArgTypes();
  callArgs.insert(callArgs.begin(), funcType);

  auto codegenCtx = ctx;
  auto newSig = "__new__[Pointer[byte]]";
  TypeRealization::InlineMagics inlineMagicFuncs = {
      {newSig,
       [llvmType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateBitCast(args[0], llvmType);
       }},
      {getMagicSignature("__str__", {funcType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return codegenCtx->codegenStr(args[0], "generator", builder.GetInsertBlock());
       }},
      {getMagicSignature("__call__", callArgs),
       [](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateCall(args[0],
                                   std::vector<Value *>(args.begin() + 1, args.end()));
       }},
  };
  auto typeRealization = std::make_shared<TypeRealization>(
      funcType, llvmType, dfltBuilder, inlineMagicFuncs, newSig);
  ctx->registerType(funcType, typeRealization);
  typeResult = typeRealization;
}

void CodegenVisitor::visit(std::shared_ptr<types::PartialFuncType> partialFuncType) {
  if (auto real = ctx->getTypeRealization(partialFuncType)) {
    typeResult = real;
    return;
  }

  auto *llvmType = StructType::get(ctx->getLLVMContext());

  auto calleeRealization = transform(partialFuncType->getCallee());

  std::vector<Type *> body;
  std::vector<std::shared_ptr<TypeRealization>> bodyRealizations;
  body.push_back(calleeRealization->llvmType);

  for (auto &bodyType : partialFuncType->getCallTypes())
    if (bodyType) {
      auto realization = transform(bodyType);
      body.push_back(realization->llvmType);
      bodyRealizations.push_back(realization);
    }

  llvmType->setBody(body);
  llvmType->setName(partialFuncType->getName());

  auto dfltBuilder = [bodyRealizations, llvmType](IRBuilder<> &builder) -> Value * {
    Value *self = UndefValue::get(llvmType);
    auto i = 0;
    for (auto &real : bodyRealizations) {
      self = builder.CreateInsertValue(self, real->getDefaultValue(builder), i++);
    }
    return self;
  };

  auto typeRealization =
      std::make_shared<TypeRealization>(partialFuncType, llvmType, dfltBuilder);
  ctx->registerType(partialFuncType, typeRealization);
  typeResult = typeRealization;
}

void CodegenVisitor::visit(std::shared_ptr<types::Optional> optType) {
  if (auto real = ctx->getTypeRealization(optType)) {
    typeResult = real;
    return;
  }

  auto baseType = optType->getBase();
  auto baseRealization = transform(baseType);
  auto *llvmType = optType->getBase()->isRef()
                       ? baseRealization->llvmType
                       : StructType::get(IntegerType::getInt1Ty(ctx->getLLVMContext()),
                                         baseRealization->llvmType);

  auto codegenCtx = ctx;
  auto dfltBuilder = [=](IRBuilder<> &builder) -> Value * {
    if (baseType->isRef()) {
      return ConstantPointerNull::get(cast<PointerType>(llvmType));
    } else {
      Value *self = UndefValue::get(llvmType);
      self = builder.CreateInsertValue(
          self,
          ConstantInt::get(IntegerType::getInt1Ty(codegenCtx->getLLVMContext()), 0), 0);
      return self;
    }
  };

  auto newSig = getMagicSignature("__new__", {optType->getBase()});
  TypeRealization::InlineMagics inlineMagics = {
      {newSig,
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         if (optType->getBase()->isRef())
           return args[0];
         else {
           Value *self = UndefValue::get(llvmType);
           self = builder.CreateInsertValue(
               self,
               ConstantInt::get(IntegerType::getInt1Ty(codegenCtx->getLLVMContext()),
                                1),
               0);
           self = builder.CreateInsertValue(self, args[0], 1);
           return self;
         }
       }},
      {getMagicSignature("__new__", {}),
       [=](std::vector<Value *>, IRBuilder<> &builder) -> Value * {
         return dfltBuilder(builder);
       }},
      {getMagicSignature("__bool__", {optType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         if (optType->getBase()->isRef()) {
           return builder.CreateICmpNE(
               args[0], ConstantPointerNull::get(cast<PointerType>(llvmType)));
         } else {
           return builder.CreateZExt(
               builder.CreateExtractValue(args[0], 0),
               codegenCtx->getTypeRealization(types::kBoolType)->llvmType);
         }
       }},
      {getMagicSignature("__invert__", {optType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return optType->getBase()->isRef() ? args[0]
                                            : builder.CreateExtractValue(args[0], 1);
       }},
  };

  TypeRealization::Fields fields = {{"has", 0}, {"val", 1}};

  TypeRealization::CustomGetters customGetters;
  if (optType->getBase()->isRef()) {
    customGetters["has"] = [llvmType](llvm::Value *self,
                                      llvm::IRBuilder<> &builder) -> Value * {
      return builder.CreateICmpEQ(
          self, ConstantPointerNull::get(cast<PointerType>(llvmType)));
    };
    customGetters["val"] = [](llvm::Value *self, llvm::IRBuilder<> &) -> Value * {
      return self;
    };
  }

  auto typeRealization = std::make_shared<TypeRealization>(
      optType, llvmType, dfltBuilder, inlineMagics, newSig,
      TypeRealization::NonInlineMagics(), fields, customGetters);
  ctx->registerType(optType, typeRealization);
  typeResult = typeRealization;
}

void CodegenVisitor::visit(std::shared_ptr<types::Array> arrayType) {
  if (auto real = ctx->getTypeRealization(arrayType)) {
    typeResult = real;
    return;
  }

  auto baseType = arrayType->getBase();
  auto baseTypeRealization = transform(baseType);
  auto *baseLLVMType = baseTypeRealization->llvmType;

  auto *llvmType = StructType::get(seqIntLLVM(ctx->getLLVMContext()),
                                   PointerType::get(baseLLVMType, 0));
  auto codegenCtx = ctx;
  auto dfltBuilder = [=](IRBuilder<> &builder) -> Value * {
    auto &context = codegenCtx->getLLVMContext();
    Value *self = UndefValue::get(llvmType);
    self = builder.CreateInsertValue(self, zeroLLVM(context), 0);
    self = builder.CreateInsertValue(
        self, ConstantPointerNull::get(PointerType::get(baseLLVMType, 0)), 1);
    return self;
  };

  auto newSig =
      fmt::format(FMT_STRING("__new__[Pointer[{}], int]"), baseType->getName());

  TypeRealization::InlineMagics inlineMagics = {
      {getMagicSignature("__new__", {types::kIntType}),
       [llvmType, baseTypeRealization](std::vector<Value *> args,
                                       IRBuilder<> &builder) -> Value * {
         auto *ptr = baseTypeRealization->alloc(args[0], builder, false);
         Value *self = UndefValue::get(llvmType);
         self = builder.CreateInsertValue(self, args[0], 0);
         self = builder.CreateInsertValue(self, ptr, 1);
         return self;
       }},
      {newSig,
       [llvmType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         Value *self = UndefValue::get(llvmType);
         self = builder.CreateInsertValue(self, args[1], 0);
         self = builder.CreateInsertValue(self, args[0], 1);
         return self;
       }},
      {getMagicSignature("__copy__", {arrayType}),
       [llvmType, baseTypeRealization, baseLLVMType,
        codegenCtx](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         Value *len = builder.CreateExtractValue(args[0], 0);
         Value *otherPtr = builder.CreateExtractValue(args[0], 1);

         auto *ptr = baseTypeRealization->alloc(len, builder, false);

         Value *self = UndefValue::get(llvmType);
         self = builder.CreateInsertValue(self, len, 0);
         self = builder.CreateInsertValue(self, ptr, 1);

         auto size =
             codegenCtx->getModule()->getDataLayout().getTypeAllocSize(llvmType);
         auto *numBytes = builder.CreateMul(
             ConstantInt::get(seqIntLLVM(codegenCtx->getLLVMContext()), size), len);
         makeMemCpy(ptr, otherPtr, numBytes, builder.GetInsertBlock());

         return self;
       }},
      {getMagicSignature("__len__", {arrayType}),
       [](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateExtractValue(args[0], 0);
       }},
      {getMagicSignature("__bool__", {arrayType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         Value *len = builder.CreateExtractValue(args[0], 0);
         Value *zero = ConstantInt::get(seqIntLLVM(codegenCtx->getLLVMContext()), 0);
         return builder.CreateZExt(
             builder.CreateICmpNE(len, zero),
             codegenCtx->getTypeRealization(types::kBoolType)->llvmType);
       }},
      {getMagicSignature("__getitem__", {arrayType, types::kIntType}),
       [](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         Value *ptr = builder.CreateExtractValue(args[0], 1);
         ptr = builder.CreateGEP(ptr, args[1]);
         return builder.CreateLoad(ptr);
       }},
      {getMagicSignature("__slice__", {arrayType, types::kIntType, types::kIntType}),
       [llvmType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         Value *ptr = builder.CreateExtractValue(args[0], 1);
         ptr = builder.CreateGEP(ptr, args[1]);

         Value *len = builder.CreateSub(args[2], args[1]);

         Value *slice = UndefValue::get(llvmType);

         slice = builder.CreateInsertValue(slice, len, 0);
         slice = builder.CreateInsertValue(slice, ptr, 1);
         return slice;
       }},
      {getMagicSignature("__slice_left__", {arrayType, types::kIntType}),
       [llvmType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         Value *ptr = builder.CreateExtractValue(args[0], 1);

         Value *slice = UndefValue::get(llvmType);

         slice = builder.CreateInsertValue(slice, args[1], 0);
         slice = builder.CreateInsertValue(slice, ptr, 1);
         return slice;
       }},
      {getMagicSignature("__slice_right__", {arrayType, types::kIntType}),
       [llvmType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
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
       [](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         Value *ptr = builder.CreateExtractValue(args[0], 1);
         ptr = builder.CreateGEP(ptr, args[1]);
         builder.CreateStore(args[2], ptr);
         return nullptr;
       }},
  };
  TypeRealization::Fields fields = {{"len", 0}, {"ptr", 1}};

  auto typeRealization = std::make_shared<TypeRealization>(
      arrayType, llvmType, dfltBuilder, inlineMagics, newSig,
      TypeRealization::NonInlineMagics(), fields);
  ctx->registerType(arrayType, typeRealization);
  typeResult = typeRealization;
}

void CodegenVisitor::visit(std::shared_ptr<types::Pointer> pointerType) {
  if (auto real = ctx->getTypeRealization(pointerType)) {
    typeResult = real;
    return;
  }

  auto baseType = pointerType->getBase();
  auto baseTypeRealization = transform(baseType);
  auto *baseLLVMType = baseTypeRealization->llvmType;

  auto *llvmType = PointerType::get(baseLLVMType, 0);

  auto dfltBuilder = [llvmType](IRBuilder<> &builder) -> Value * {
    return ConstantPointerNull::get(llvmType);
  };
  auto newSig = "__new__[Pointer[byte]]";
  auto codegenCtx = ctx;
  TypeRealization::InlineMagics inlineMagics = {
      {getMagicSignature("__elemsize__", {}),
       [baseLLVMType, codegenCtx](std::vector<Value *> args,
                                  IRBuilder<> &builder) -> Value * {
         return ConstantInt::get(
             seqIntLLVM(codegenCtx->getLLVMContext()),
             codegenCtx->getModule()->getDataLayout().getTypeAllocSize(baseLLVMType));
       }},
      {getMagicSignature("__atomic__", {}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return ConstantInt::get(
             codegenCtx->getTypeRealization(types::kBoolType)->llvmType,
             baseType->isAtomic() ? 1 : 0);
       }},
      {getMagicSignature("__new__", {}),
       [dfltBuilder](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return dfltBuilder(builder);
       }},
      {getMagicSignature("__new__", {types::kIntType}),
       [baseTypeRealization](std::vector<Value *> args, IRBuilder<> &builder)
           -> Value * { return baseTypeRealization->alloc(args[0], builder, false); }},
      {newSig,
       [llvmType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateBitCast(args[0], llvmType);
       }},
      {getMagicSignature("as_byte", {pointerType}),
       [](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateBitCast(args[0], builder.getInt8PtrTy());
       }},
      {getMagicSignature("__int__", {pointerType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreatePtrToInt(args[0],
                                       seqIntLLVM(codegenCtx->getLLVMContext()));
       }},
      {getMagicSignature("__copy__", {pointerType}),
       [](std::vector<Value *> args, IRBuilder<> &) -> Value * { return args[0]; }},
      {getMagicSignature("__bool__", {pointerType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateZExt(
             builder.CreateIsNotNull(args[0]),
             codegenCtx->getTypeRealization(types::kBoolType)->llvmType);
       }},
      {getMagicSignature("__getitem__", {pointerType, types::kIntType}),
       [](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         Value *ptr = builder.CreateGEP(args[0], args[1]);
         return builder.CreateLoad(ptr);
       }},
      {getMagicSignature("__setitem__", {pointerType, types::kIntType, baseType}),
       [](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         Value *ptr = builder.CreateGEP(args[0], args[1]);
         builder.CreateStore(args[2], ptr);
         return nullptr;
       }},
      {getMagicSignature("__add__", {pointerType, types::kIntType}),
       [](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateGEP(args[0], args[1]);
       }},
      {getMagicSignature("__sub__", {pointerType, pointerType}),
       [](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreatePtrDiff(args[0], args[1]);
       }},
      {getMagicSignature("__eq__", {pointerType, pointerType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateZExt(
             builder.CreateICmpEQ(args[0], args[1]),
             codegenCtx->getTypeRealization(types::kBoolType)->llvmType);
       }},
      {getMagicSignature("__ne__", {pointerType, pointerType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateZExt(
             builder.CreateICmpNE(args[0], args[1]),
             codegenCtx->getTypeRealization(types::kBoolType)->llvmType);
       }},
      {getMagicSignature("__lt__", {pointerType, pointerType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateZExt(
             builder.CreateICmpSLT(args[0], args[1]),
             codegenCtx->getTypeRealization(types::kBoolType)->llvmType);
       }},
      {getMagicSignature("__gt__", {pointerType, pointerType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateZExt(
             builder.CreateICmpSGT(args[0], args[1]),
             codegenCtx->getTypeRealization(types::kBoolType)->llvmType);
       }},
      {getMagicSignature("__le__", {pointerType, pointerType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateZExt(
             builder.CreateICmpSLE(args[0], args[1]),
             codegenCtx->getTypeRealization(types::kBoolType)->llvmType);
       }},
      {getMagicSignature("__ge__", {pointerType, pointerType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateZExt(
             builder.CreateICmpSGE(args[0], args[1]),
             codegenCtx->getTypeRealization(types::kBoolType)->llvmType);
       }},

      /*
       * Prefetch magics are labeled [rw][0123] representing read/write and
       * locality. Instruction cache prefetch is not supported.
       * https://llvm.org/docs/LangRef.html#llvm-prefetch-intrinsic
       */
      {getMagicSignature("__prefetch_r0__", {pointerType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         auto *self = builder.CreateBitCast(args[0], builder.getInt8PtrTy());
         Function *prefetch =
             Intrinsic::getDeclaration(codegenCtx->getModule(), Intrinsic::prefetch);
         builder.CreateCall(prefetch, {self, builder.getInt32(0), builder.getInt32(0),
                                       builder.getInt32(1)});
         return nullptr;
       }},
      {getMagicSignature("__prefetch_r1__", {pointerType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         auto *self = builder.CreateBitCast(args[0], builder.getInt8PtrTy());
         Function *prefetch =
             Intrinsic::getDeclaration(codegenCtx->getModule(), Intrinsic::prefetch);
         builder.CreateCall(prefetch, {self, builder.getInt32(0), builder.getInt32(1),
                                       builder.getInt32(1)});
         return nullptr;
       }},
      {getMagicSignature("__prefetch_r2__", {pointerType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         auto *self = builder.CreateBitCast(args[0], builder.getInt8PtrTy());
         Function *prefetch =
             Intrinsic::getDeclaration(codegenCtx->getModule(), Intrinsic::prefetch);
         builder.CreateCall(prefetch, {self, builder.getInt32(0), builder.getInt32(2),
                                       builder.getInt32(1)});
         return nullptr;
       }},
      {getMagicSignature("__prefetch_r3__", {pointerType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         auto *self = builder.CreateBitCast(args[0], builder.getInt8PtrTy());
         Function *prefetch =
             Intrinsic::getDeclaration(codegenCtx->getModule(), Intrinsic::prefetch);
         builder.CreateCall(prefetch, {self, builder.getInt32(0), builder.getInt32(3),
                                       builder.getInt32(1)});
         return nullptr;
       }},
      {getMagicSignature("__prefetch_w0__", {pointerType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         auto *self = builder.CreateBitCast(args[0], builder.getInt8PtrTy());
         Function *prefetch =
             Intrinsic::getDeclaration(codegenCtx->getModule(), Intrinsic::prefetch);
         builder.CreateCall(prefetch, {self, builder.getInt32(1), builder.getInt32(0),
                                       builder.getInt32(1)});
         return nullptr;
       }},
      {getMagicSignature("__prefetch_w1__", {pointerType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         auto *self = builder.CreateBitCast(args[0], builder.getInt8PtrTy());
         Function *prefetch =
             Intrinsic::getDeclaration(codegenCtx->getModule(), Intrinsic::prefetch);
         builder.CreateCall(prefetch, {self, builder.getInt32(1), builder.getInt32(1),
                                       builder.getInt32(1)});
         return nullptr;
       }},
      {getMagicSignature("__prefetch_w2__", {pointerType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         auto *self = builder.CreateBitCast(args[0], builder.getInt8PtrTy());
         Function *prefetch =
             Intrinsic::getDeclaration(codegenCtx->getModule(), Intrinsic::prefetch);
         builder.CreateCall(prefetch, {self, builder.getInt32(1), builder.getInt32(2),
                                       builder.getInt32(1)});
         return nullptr;
       }},
      {getMagicSignature("__prefetch_w3__", {pointerType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         auto *self = builder.CreateBitCast(args[0], builder.getInt8PtrTy());
         Function *prefetch =
             Intrinsic::getDeclaration(codegenCtx->getModule(), Intrinsic::prefetch);
         builder.CreateCall(prefetch, {self, builder.getInt32(1), builder.getInt32(3),
                                       builder.getInt32(1)});
         return nullptr;
       }},
  };
  auto typeRealization = std::make_shared<TypeRealization>(
      pointerType, llvmType, dfltBuilder, inlineMagics, newSig);
  ctx->registerType(pointerType, typeRealization);
  typeResult = typeRealization;
}

void CodegenVisitor::visit(std::shared_ptr<types::Generator> genType) {
  if (auto real = ctx->getTypeRealization(genType)) {
    typeResult = real;
    return;
  }

  auto baseType = genType->getBase();
  auto baseTypeRealization = transform(baseType);
  auto *baseLLVMType = baseTypeRealization->llvmType;

  auto *llvmType = IntegerType::getInt8PtrTy(ctx->getLLVMContext());

  auto dfltBuilder = [llvmType](IRBuilder<> &builder) -> Value * {
    return ConstantPointerNull::get(llvmType);
  };
  auto codegenCtx = ctx;
  TypeRealization::InlineMagics inlineMagics = {
      {getMagicSignature("__iter__", {genType}),
       [](std::vector<Value *> args, IRBuilder<> &) -> Value * { return args[0]; }},
      {getMagicSignature("__raw__", {genType}),
       [](std::vector<Value *> args, IRBuilder<> &) -> Value * { return args[0]; }},
      {getMagicSignature("__done__", {genType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateZExt(
             generatorDone(args[0], builder.GetInsertBlock()),
             codegenCtx->getTypeRealization(types::kBoolType)->llvmType);
       }},
      {getMagicSignature("done", {genType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         generatorResume(args[0], builder.GetInsertBlock(), nullptr, nullptr);
         return builder.CreateZExt(
             generatorDone(args[0], builder.GetInsertBlock()),
             codegenCtx->getTypeRealization(types::kBoolType)->llvmType);
       }},
      {getMagicSignature("__promise__", {genType}),
       [baseLLVMType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return generatorPromise(args[0], builder.GetInsertBlock(), baseLLVMType, true);
       }},
      {getMagicSignature("__resume__", {genType}),
       [](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         generatorResume(args[0], builder.GetInsertBlock(), nullptr, nullptr);
         return nullptr;
       }},
      {getMagicSignature("send", {genType, baseType}),
       [baseLLVMType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         generatorSend(args[0], args[1], builder.GetInsertBlock(), baseLLVMType);
         generatorResume(args[0], builder.GetInsertBlock(), nullptr, nullptr);
         return generatorPromise(args[0], builder.GetInsertBlock(), baseLLVMType);
       }},
      {getMagicSignature("__str__", {genType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return codegenCtx->codegenStr(args[0], "generator", builder.GetInsertBlock());
       }},
      {getMagicSignature("destroy", {genType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         generatorDestroy(args[0], builder.GetInsertBlock());
         return nullptr;
       }},
  };

  TypeRealization::NonInlineMagics nonInlineMagicFuncs = {
      {getMagicSignature("next", {genType}), [=](Function *func) {
         func->setLinkage(GlobalValue::PrivateLinkage);
         func->setDoesNotThrow();
         func->setPersonalityFn(makePersonalityFunc(codegenCtx->getModule()));
         func->addFnAttr(llvm::Attribute::AlwaysInline);

         Value *arg = func->arg_begin();
         auto *entry =
             llvm::BasicBlock::Create(codegenCtx->getLLVMContext(), "entry", func);
         auto *val = generatorPromise(arg, entry, baseLLVMType);
         IRBuilder<> builder(entry);
         builder.CreateRet(val);
       }}};

  auto typeRealization = std::make_shared<TypeRealization>(
      genType, llvmType, dfltBuilder, inlineMagics, "", nonInlineMagicFuncs);
  ctx->registerType(genType, typeRealization);
  typeResult = typeRealization;
}

void CodegenVisitor::visit(std::shared_ptr<types::IntNType> intNType) {
  auto *llvmType = IntegerType::getIntNTy(ctx->getLLVMContext(), intNType->getLen());
  if (auto real = ctx->getTypeRealization(intNType)) {
    typeResult = real;
    return;
  }

  auto dfltBuilder = [llvmType](IRBuilder<> &builder) -> Value * {
    return ConstantInt::get(llvmType, 0);
  };

  auto newSig = "__new__[int]";
  auto codegenCtx = ctx;
  TypeRealization::InlineMagics inlineMagicFuncs = {
      {"__new__[]",
       [dfltBuilder](std::vector<Value *>, IRBuilder<> &builder) -> Value * {
         return dfltBuilder(builder);
       }},
      {newSig,
       [intNType, llvmType](std::vector<Value *> args,
                            IRBuilder<> &builder) -> Value * {
         return intNType->isSigned() ? builder.CreateSExtOrTrunc(args[0], llvmType)
                                     : builder.CreateZExtOrTrunc(args[0], llvmType);
       }},
      {getMagicSignature("__new__", {intNType}),
       [=](std::vector<Value *> args, IRBuilder<> &) -> Value * { return args[0]; }},
      {fmt::format(FMT_STRING("__new__[{}]"), intNType->oppositeSignName()),
       [=](std::vector<Value *> args, IRBuilder<> &) -> Value * { return args[0]; }},
      {getMagicSignature("__int__", {intNType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return intNType->isSigned()
                    ? builder.CreateSExtOrTrunc(
                          args[0],
                          codegenCtx->getTypeRealization(types::kIntType)->llvmType)
                    : builder.CreateZExtOrTrunc(
                          args[0],
                          codegenCtx->getTypeRealization(types::kIntType)->llvmType);
       }},
      {getMagicSignature("__copy__", {intNType}),
       [](std::vector<Value *> args, IRBuilder<> &) -> Value * { return args[0]; }},
      {getMagicSignature("__hash__", {intNType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return intNType->isSigned()
                    ? builder.CreateSExtOrTrunc(
                          args[0], seqIntLLVM(codegenCtx->getLLVMContext()))
                    : builder.CreateZExtOrTrunc(
                          args[0], seqIntLLVM(codegenCtx->getLLVMContext()));
       }},
      {getMagicSignature("__bool__", {intNType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateZExt(
             builder.CreateICmpNE(args[0], ConstantInt::get(llvmType, 0)),
             codegenCtx->getTypeRealization(types::kBoolType)->llvmType);
       }},
      {getMagicSignature("__pos__", {intNType}),
       [](std::vector<Value *> args, IRBuilder<> &) -> Value * { return args[0]; }},
      {getMagicSignature("__neg__", {intNType}),
       [](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateNeg(args[0]);
       }},
      {getMagicSignature("__invert__", {intNType}),
       [](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateNot(args[0]);
       }},
      // int-int binary
      {getMagicSignature("__add__", {intNType, intNType}),
       [](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateAdd(args[0], args[1]);
       }},
      {getMagicSignature("__sub__", {intNType, intNType}),
       [](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateSub(args[0], args[1]);
       }},
      {getMagicSignature("__mul__", {intNType, intNType}),
       [](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateMul(args[0], args[1]);
       }},
      {getMagicSignature("__truediv__", {intNType, intNType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         auto *floatType = codegenCtx->getTypeRealization(types::kFloatType)->llvmType;

         args[0] = intNType->isSigned() ? builder.CreateSIToFP(args[0], floatType)
                                        : builder.CreateUIToFP(args[0], floatType);
         args[1] = intNType->isSigned() ? builder.CreateSIToFP(args[1], floatType)
                                        : builder.CreateUIToFP(args[1], floatType);
         return builder.CreateFDiv(args[0], args[1]);
       }},
      {getMagicSignature("__div__", {intNType, intNType}),
       [](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateSDiv(args[0], args[1]);
       }},
      {getMagicSignature("__mod__", {intNType, intNType}),
       [](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateSRem(args[0], args[1]);
       }},
      {getMagicSignature("__lshift__", {intNType, intNType}),
       [](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return builder.CreateShl(args[0], args[1]);
       }},
      {getMagicSignature("__rshift__", {intNType, intNType}),
       [intNType](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         return intNType->isSigned() ? builder.CreateAShr(args[0], args[1])
                                     : builder.CreateLShr(args[0], args[1]);
       }},
      {getMagicSignature("__eq__", {intNType, intNType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         auto *boolType = codegenCtx->getTypeRealization(types::kBoolType)->llvmType;
         return builder.CreateZExt(builder.CreateICmpEQ(args[0], args[1]), boolType);
       }},
      {getMagicSignature("__ne__", {intNType, intNType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         auto *boolType = codegenCtx->getTypeRealization(types::kBoolType)->llvmType;
         return builder.CreateZExt(builder.CreateICmpNE(args[0], args[1]), boolType);
       }},
      {getMagicSignature("__lt__", {intNType, intNType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         auto *boolType = codegenCtx->getTypeRealization(types::kBoolType)->llvmType;
         auto *value = intNType->isSigned() ? builder.CreateICmpSLT(args[0], args[1])
                                            : builder.CreateICmpULT(args[0], args[1]);
         return builder.CreateZExt(value, boolType);
       }},
      {getMagicSignature("__gt__", {intNType, intNType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         auto *boolType = codegenCtx->getTypeRealization(types::kBoolType)->llvmType;
         auto *value = intNType->isSigned() ? builder.CreateICmpSGT(args[0], args[1])
                                            : builder.CreateICmpUGT(args[0], args[1]);
         return builder.CreateZExt(value, boolType);
       }},
      {getMagicSignature("__le__", {intNType, intNType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         auto *boolType = codegenCtx->getTypeRealization(types::kBoolType)->llvmType;
         auto *value = intNType->isSigned() ? builder.CreateICmpSLE(args[0], args[1])
                                            : builder.CreateICmpULE(args[0], args[1]);
         return builder.CreateZExt(value, boolType);
       }},
      {getMagicSignature("__ge__", {intNType, intNType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         auto *boolType = codegenCtx->getTypeRealization(types::kBoolType)->llvmType;
         auto *value = intNType->isSigned() ? builder.CreateICmpSGE(args[0], args[1])
                                            : builder.CreateICmpUGE(args[0], args[1]);
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
         auto *gzWrite = cast<Function>(codegenCtx->getModule()->getOrInsertFunction(
             "gzwrite", IntegerType::getInt32Ty(codegenCtx->getLLVMContext()),
             IntegerType::getInt8PtrTy(codegenCtx->getLLVMContext()),
             IntegerType::getInt8PtrTy(codegenCtx->getLLVMContext()),
             IntegerType::getInt32Ty(codegenCtx->getLLVMContext())));
         gzWrite->setDoesNotThrow();
         Value *buf = builder.CreateAlloca(llvmType);
         builder.CreateStore(args[0], buf);
         Value *ptr = builder.CreateBitCast(
             buf, IntegerType::getInt8PtrTy(codegenCtx->getLLVMContext()));
         Value *size = ConstantInt::get(
             seqIntLLVM(codegenCtx->getLLVMContext()),
             codegenCtx->getModule()->getDataLayout().getTypeAllocSize(llvmType));
         builder.CreateCall(gzWrite, {args[1], ptr, size});
         return nullptr;
       }},
      {"__unpickle__[Pointer[byte]]",
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         auto *gzRead = cast<Function>(codegenCtx->getModule()->getOrInsertFunction(
             "gzread", IntegerType::getInt32Ty(codegenCtx->getLLVMContext()),
             IntegerType::getInt8PtrTy(codegenCtx->getLLVMContext()),
             IntegerType::getInt8PtrTy(codegenCtx->getLLVMContext()),
             IntegerType::getInt32Ty(codegenCtx->getLLVMContext())));
         gzRead->setDoesNotThrow();
         Value *buf = builder.CreateAlloca(llvmType);
         Value *ptr = builder.CreateBitCast(
             buf, IntegerType::getInt8PtrTy(codegenCtx->getLLVMContext()));
         Value *size = ConstantInt::get(
             seqIntLLVM(codegenCtx->getLLVMContext()),
             codegenCtx->getModule()->getDataLayout().getTypeAllocSize(llvmType));
         builder.CreateCall(gzRead, {args[0], ptr, size});
         return builder.CreateLoad(buf);
       }},
      {getMagicSignature("popcnt", {intNType}),
       [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
         Function *popcnt = Intrinsic::getDeclaration(codegenCtx->getModule(),
                                                      Intrinsic::ctpop, {llvmType});
         Value *count = builder.CreateCall(args[0]);
         return builder.CreateZExtOrTrunc(count,
                                          seqIntLLVM(codegenCtx->getLLVMContext()));
       }},
  };

  auto typeRealization = std::make_shared<TypeRealization>(
      intNType, llvmType, dfltBuilder, inlineMagicFuncs, newSig);

  if (intNType->getLen() <= 64) {
    typeRealization->inlineMagicFuncs[getMagicSignature("__str__", {intNType})] =
        [=](std::vector<Value *> args, IRBuilder<> &builder) -> Value * {
      auto *self = typeRealization->callMagic(getMagicSignature("__int__", {intNType}),
                                              {args[0]}, builder);
      return codegenCtx->getTypeRealization(types::kIntType)
          ->callMagic("__str__[int]", {self}, builder);
    };
  }
  ctx->registerType(intNType, typeRealization);
  typeResult = typeRealization;
}

void CodegenVisitor::codegenTcJump(llvm::BasicBlock *dst,
                                   std::vector<std::shared_ptr<TryCatch>> tcPath) {
  IRBuilder<> builder(ctx->getFrame().curBlock);
  if (tcPath.empty()) {
    builder.CreateBr(dst);
    return;
  }

  std::shared_ptr<TryCatchMetadata> firstMeta;
  auto curMeta = firstMeta;

  for (auto &it : tcPath) {
    auto meta = transform(it);
    if (!meta->finallyStart)
      continue;
    if (!firstMeta) {
      firstMeta = meta;
      curMeta = meta;
    }

    builder.SetInsertPoint(ctx->getFrame().curBlock);
    curMeta->storeDstValue(meta->finallyStart, builder);
    curMeta = meta;
  }

  if (firstMeta->finallyStart) {
    builder.SetInsertPoint(ctx->getFrame().curBlock);
    curMeta->storeDstValue(dst, builder);

    builder.CreateBr(firstMeta->finallyStart);
  } else {
    builder.CreateBr(dst);
  }
}

} // namespace codegen
} // namespace ir
} // namespace seq