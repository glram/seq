#pragma once

#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "util/llvm.h"

namespace seq {
namespace ir {

class Func;
class BasicBlock;
class Var;

namespace types {
class Type;
}

namespace codegen {

struct FuncMetadata {
  llvm::Function *func;

  bool isGenerator;
  /// Storage for this coroutine's promise, or null if none
  llvm::Value *promise;

  /// Coroutine handle, or null if none
  llvm::Value *handle;

  /// Coroutine cleanup block, or null if none
  llvm::BasicBlock *cleanup;

  /// Coroutine suspend block, or null if none
  llvm::BasicBlock *suspend;

  /// Coroutine exit block, or null if none
  llvm::BasicBlock *exit;
};

struct Frame {
  std::unordered_map<int, llvm::Value *> varRealizations;
  std::unordered_map<int, llvm::BasicBlock *> blockRealizations;
  llvm::BasicBlock *curBlock = nullptr;
  FuncMetadata funcMetadata = {nullptr, false,   nullptr, nullptr,
                               nullptr, nullptr, nullptr};
};

class Context {
public:
  using DefaultValueBuilder = std::function<llvm::Value *(llvm::IRBuilder<> &)>;

  using LLVMValueBuilder =
      std::function<llvm::Value *(std::vector<llvm::Value *>, llvm::IRBuilder<> &)>;
  using InlineMagicFuncs = std::unordered_map<std::string, LLVMValueBuilder>;

  using LLVMFuncBuilder = std::function<void(llvm::Function *)>;
  using NonInlineMagicFuncs = std::unordered_map<std::string, LLVMFuncBuilder>;

private:
  llvm::Module *module;
  std::unordered_map<int, llvm::Type *> typeRealizations;
  std::unordered_map<int, DefaultValueBuilder> defaultValueBuilders;
  std::unordered_map<int, InlineMagicFuncs> inlineMagicBuilders;
  std::unordered_map<int, NonInlineMagicFuncs> nonInlineMagicFuncs;

  std::vector<Frame> frames;

  int seqMainId;

  void initTypeRealizations();
  void initMagicFuncs();

public:
  explicit Context(llvm::Module *module) : module(module), seqMainId(-1) {
    initTypeRealizations();
    initMagicFuncs();
  }

  void registerType(std::shared_ptr<types::Type> sirType, llvm::Type *llvmType,
                    DefaultValueBuilder dfltBuilder, InlineMagicFuncs inlineMagics = {},
                    NonInlineMagicFuncs nonInlineMagics = {});

  void registerVar(std::shared_ptr<Var> sirVar, llvm::Value *val);
  void registerBlock(std::shared_ptr<BasicBlock> sirBlock, llvm::BasicBlock *block);

  void pushFrame() { frames.emplace_back(); }
  Frame &getFrame() { return frames.back(); }
  void popFrame() { frames.pop_back(); }

  llvm::Value *getVar(std::shared_ptr<Var> sirVar);
  llvm::BasicBlock *getBlock(std::shared_ptr<BasicBlock> sirBlock);

  llvm::Type *getLLVMType(std::shared_ptr<types::Type> type);

  llvm::Value *getDefaultValue(std::shared_ptr<types::Type> type,
                               llvm::IRBuilder<> &builder);

  llvm::Module *getModule() { return module; }
  llvm::LLVMContext &getLLVMContext() { return module->getContext(); }

  LLVMFuncBuilder getMagicBuilder(std::shared_ptr<types::Type> type,
                                  const std::string &sig);
};

} // namespace codegen
} // namespace ir
} // namespace seq
