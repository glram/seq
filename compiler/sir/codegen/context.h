#pragma once

#include <memory>
#include <unordered_map>
#include <utility>

#include "util/llvm.h"

namespace seq {
namespace ir {

namespace types {
class Type;
}

namespace codegen {

class Context {
public:
  using DefaultValueBuilder = std::function<llvm::Value *(llvm::IRBuilder<> &)>;

  using LLVMValueBuilder =
      std::function<llvm::Value *(std::vector<llvm::Value *>, llvm::IRBuilder<> &)>;
  using InlineMagicFuncs = std::unordered_map<std::string, LLVMValueBuilder>;

  using NonInlineMagicFuncs = std::unordered_map<std::string, llvm::Function *>;

private:
  llvm::Module *module;
  std::unordered_map<int, llvm::Type *> typeRealizations;
  std::unordered_map<int, DefaultValueBuilder> defaultValueBuilders;
  std::unordered_map<int, InlineMagicFuncs> inlineMagicBuilders;
  std::unordered_map<int, NonInlineMagicFuncs> nonInlineMagicFuncs;

  void initTypeRealizations();
  void initMagicFuncs();

public:
  explicit Context(llvm::Module *module) : module(module) {
    initTypeRealizations();
    initMagicFuncs();
  }

  void registerType(std::shared_ptr<types::Type> sirType, llvm::Type *llvmType,
                    DefaultValueBuilder dfltBuilder, InlineMagicFuncs inlineMagics = {},
                    NonInlineMagicFuncs nonInlineMagics = {});

  llvm::Type *getLLVMType(std::shared_ptr<types::Type> type);

  llvm::Value *getDefaultValue(std::shared_ptr<types::Type> type,
                               llvm::IRBuilder<> &builder);

  llvm::Module *getModule() { return module; }
  llvm::LLVMContext &getLLVMContext() { return module->getContext(); }
};

} // namespace codegen
} // namespace ir
} // namespace seq
