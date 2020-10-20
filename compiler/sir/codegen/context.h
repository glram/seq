#pragma once

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "sir/common/context.h"

#include "util/llvm.h"

namespace seq {
namespace ir {

class Func;
class BasicBlock;
class Var;
class Pattern;
class TryCatch;

namespace types {
class Type;
}

namespace codegen {

struct TryCatchMetadata {
private:
  llvm::SwitchInst *finallyBr = nullptr;

public:
  llvm::BasicBlock *exceptionBlock = nullptr;
  llvm::BasicBlock *exceptionRouteBlock = nullptr;
  llvm::BasicBlock *finallyStart = nullptr;
  std::vector<llvm::BasicBlock *> handlers;
  llvm::Value *excFlag = nullptr;
  llvm::Value *catchStore = nullptr;

  void storeDstValue(llvm::BasicBlock *dst, llvm::IRBuilder<> &builder);
};

struct CodegenFrame {
  std::unordered_map<int, llvm::Value *> varRealizations;
  std::unordered_map<int, llvm::BasicBlock *> blockRealizations;

  llvm::BasicBlock *curBlock = nullptr;
  std::shared_ptr<BasicBlock> curIRBlock;

  llvm::Function *func = nullptr;
  std::unordered_map<int, std::shared_ptr<TryCatchMetadata>> tryCatchMeta;

  bool isGenerator = false;

  llvm::Value *rValPtr = nullptr;

  // Storage for this coroutine's promise, or null if none
  llvm::Value *promise = nullptr;

  // Coroutine handle, or null if none
  llvm::Value *handle = nullptr;

  // Coroutine cleanup block, or null if none
  llvm::BasicBlock *cleanup = nullptr;

  // Coroutine suspend block, or null if none
  llvm::BasicBlock *suspend = nullptr;

  // Coroutine exit block, or null if none
  // If not a coroutine, this is the return block
  llvm::BasicBlock *exit = nullptr;
};

struct TypeRealization {
public:
  std::shared_ptr<seq::ir::types::Type> irType;
  llvm::Type *llvmType;

  using Fields = std::unordered_map<std::string, int>;
  Fields fields;

  using CustomGetterFunc =
      std::function<llvm::Value *(llvm::Value *, llvm::IRBuilder<> &)>;
  using CustomGetters = std::unordered_map<std::string, CustomGetterFunc>;
  CustomGetters customGetters;

  using MemberPointerFunc =
      std::function<llvm::Value *(llvm::Value *, int, llvm::IRBuilder<> &)>;
  MemberPointerFunc memberPointerFunc;

  using DefaultBuilder = std::function<llvm::Value *(llvm::IRBuilder<> &)>;
  DefaultBuilder dfltBuilder;

  using InlineMagicBuilder =
      std::function<llvm::Value *(std::vector<llvm::Value *>, llvm::IRBuilder<> &)>;
  using InlineMagics = std::unordered_map<std::string, InlineMagicBuilder>;
  InlineMagics inlineMagicFuncs;

  using NonInlineMagicBuilder = std::function<void(llvm::Function *)>;
  using NonInlineMagics = std::unordered_map<std::string, NonInlineMagicBuilder>;
  NonInlineMagics nonInlineMagicFuncs;

  using ReverseMagicStubs = std::unordered_map<std::string, llvm::Function *>;
  ReverseMagicStubs reverseMagicStubs;

  InlineMagicBuilder maker;

  using CustomLoader = std::function<llvm::Value *(llvm::Value *, llvm::IRBuilder<> &)>;
  CustomLoader customLoader;

public:
  TypeRealization(std::shared_ptr<seq::ir::types::Type> irType, llvm::Type *llvmType)
      : irType(std::move(irType)), llvmType(llvmType) {}
  TypeRealization(std::shared_ptr<seq::ir::types::Type> irType, llvm::Type *llvmType,
                  DefaultBuilder dfltBuilder, InlineMagics inlineMagicFuncs = {},
                  const std::string &newSig = "",
                  NonInlineMagics nonInlineMagicFuncs = {},
                  ReverseMagicStubs reverseMagicStubs = {}, Fields fields = {},
                  CustomGetters customGetters = {},
                  MemberPointerFunc memberPointerFunc = nullptr,
                  CustomLoader customLoader = nullptr)
      : irType(std::move(irType)), llvmType(llvmType), fields(std::move(fields)),
        customGetters(std::move(customGetters)),
        memberPointerFunc(std::move(memberPointerFunc)),
        dfltBuilder(std::move(dfltBuilder)),
        inlineMagicFuncs(std::move(inlineMagicFuncs)),
        nonInlineMagicFuncs(std::move(nonInlineMagicFuncs)),
        reverseMagicStubs(std::move(reverseMagicStubs)),
        maker(newSig.empty() ? nullptr : inlineMagicFuncs[newSig]),
        customLoader(std::move(customLoader)) {}

  llvm::Value *extractMember(llvm::Value *self, const std::string &field,
                             llvm::IRBuilder<> &builder) const;
  llvm::Value *getMemberPointer(llvm::Value *ptr, const std::string &field,
                                llvm::IRBuilder<> &builder) const;
  llvm::Value *getDefaultValue(llvm::IRBuilder<> &builder) const;
  llvm::Value *getUndefValue() const;
  llvm::Value *callMagic(const std::string &sig, std::vector<llvm::Value *> args,
                         llvm::IRBuilder<> &builder);
  NonInlineMagicBuilder getMagicBuilder(const std::string &sig) const;
  llvm::Function *getStub(const std::string &sig) const;
  llvm::Value *makeNew(std::vector<llvm::Value *> args,
                       llvm::IRBuilder<> &builder) const;
  llvm::Value *load(llvm::Value *ptr, llvm::IRBuilder<> &builder) const;
  llvm::Value *alloc(llvm::Value *count, llvm::IRBuilder<> &builder,
                     bool stack = false) const;
};

class Context : public seq::ir::common::IRContext<CodegenFrame> {
private:
  llvm::Module *module;
  std::unordered_map<int, std::shared_ptr<TypeRealization>> typeRealizations;

  void initTypeRealizations();

public:
  explicit Context(llvm::Module *module) : module(module) { initTypeRealizations(); }

  void registerType(std::shared_ptr<types::Type> sirType,
                    std::shared_ptr<TypeRealization> t);
  std::shared_ptr<TypeRealization>
  getTypeRealization(std::shared_ptr<types::Type> sirType);

  void registerTryCatch(std::shared_ptr<TryCatch> tc,
                        std::shared_ptr<TryCatchMetadata> meta);
  std::shared_ptr<TryCatchMetadata> getTryCatchMeta(std::shared_ptr<TryCatch> tc);

  void registerVar(std::shared_ptr<Var> sirVar, llvm::Value *val);
  void registerBlock(std::shared_ptr<BasicBlock> sirBlock, llvm::BasicBlock *block);

  llvm::Value *getVar(std::shared_ptr<Var> sirVar);
  llvm::BasicBlock *getBlock(std::shared_ptr<BasicBlock> sirBlock);

  llvm::Module *getModule() { return module; }
  llvm::LLVMContext &getLLVMContext() { return module->getContext(); }

  llvm::Value *callBuiltin(const std::string &signature,
                           std::vector<llvm::Value *> args, llvm::IRBuilder<> &builder);

  llvm::Value *codegenStr(llvm::Value *self, const std::string &name,
                          llvm::BasicBlock *block);
};

} // namespace codegen
} // namespace ir
} // namespace seq
