#pragma once

#include <memory>
#include <string>
#include <vector>

#include "util/llvm.h"

namespace seq {
namespace ir {

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
                              llvm::Type *outType, bool returnPtr = true);
void generatorSend(llvm::Value *self, llvm::Value *val, llvm::BasicBlock *block,
                   llvm::Type *outType);
void generatorDestroy(llvm::Value *self, llvm::BasicBlock *block);

void funcReturn(CodegenFrame &meta, llvm::Value *val, llvm::BasicBlock *block);
void funcYield(CodegenFrame &meta, llvm::Value *val, llvm::BasicBlock *block,
               llvm::BasicBlock *dst);

} // namespace codegen
} // namespace ir
} // namespace seq