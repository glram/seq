#pragma once

#include <memory>
#include <string>
#include <vector>

#include "util/llvm.h"

namespace seq {
namespace ir {

class IRModule;

namespace types {
class Type;
class Generator;
class MemberedType;
} // namespace types

namespace codegen {

struct CodegenFrame;

std::string getMagicSignature(const std::string &name,
                              std::vector<std::shared_ptr<types::Type>> types);

llvm::GlobalVariable *getByteCompTable(llvm::Module *module,
                                       const std::string &name = "seq.byte_comp_table");

llvm::Value *generatorDone(llvm::Value *self, llvm::BasicBlock *block);
void generatorResume(llvm::Value *self, llvm::BasicBlock *block,
                     llvm::BasicBlock *normal, llvm::BasicBlock *unwind);
llvm::Value *generatorPromise(llvm::Value *self, llvm::BasicBlock *block,
                              llvm::Type *outType, bool returnPtr = false);
void generatorSend(llvm::Value *self, llvm::Value *val, llvm::BasicBlock *block,
                   llvm::Type *outType);
void generatorDestroy(llvm::Value *self, llvm::BasicBlock *block);

void funcYield(CodegenFrame &meta, llvm::Value *val, llvm::BasicBlock *block,
               llvm::BasicBlock *dst);
void funcYieldIn(CodegenFrame &meta, llvm::Value *ptr, llvm::BasicBlock *block,
                 llvm::BasicBlock *dst);

llvm::StructType *getTypeInfoType(llvm::LLVMContext &ctx);
llvm::StructType *getPadType(llvm::LLVMContext &ctx);
llvm::StructType *getExcType(llvm::LLVMContext &ctx);

llvm::Module *compile(llvm::LLVMContext &context, std::shared_ptr<IRModule> module);

} // namespace codegen
} // namespace ir
} // namespace seq