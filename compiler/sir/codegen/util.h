#pragma once

#include <memory>
#include <string>
#include <vector>

#include "util/llvm.h"

namespace seq {
namespace ir {

class SIRModule;

namespace types {
class Type;
class GeneratorType;
class MemberedType;
} // namespace types

namespace codegen {

struct CodegenFrame;

/// Formats the signature of a magic function.
/// @param name the magic function's name
/// @param types the types of the function's arguments
/// @return the formatted signature
std::string getMagicSignature(const std::string &name,
                              std::vector<std::shared_ptr<types::Type>> types);

llvm::GlobalVariable *getByteCompTable(llvm::Module *module,
                                       const std::string &name = "seq.byte_comp_table");

/// Generates logic to detect if a generator is done.
/// @param self the generator
/// @param block the block to codegen into
/// @return an LLVM boolean that is true if the generator is done, false otherwise
llvm::Value *generatorDone(llvm::Value *self, llvm::BasicBlock *block);

/// Generates logic to resume a generator.
/// @param self the generator
/// @param block the block to codegen into
/// @param normal the normal next block, may be nullptr
/// @param unwind the handler block, may be nullptr
void generatorResume(llvm::Value *self, llvm::BasicBlock *block,
                     llvm::BasicBlock *normal, llvm::BasicBlock *unwind);

/// Generates logic to access the promise of a generator.
/// @param self the generator
/// @param block the block to codegen into
/// @param outType the LLVM type of the promise
/// @return the value of the promise if !returnPtr, otherwise a pointer to the promise
llvm::Value *generatorPromise(llvm::Value *self, llvm::BasicBlock *block,
                              llvm::Type *outType, bool returnPtr = false);

/// Generates logic to store a value to the promise of a generator.
/// @param self the generator
/// @param val the value
/// @param block the block to codegen into
/// @param outType the LLVM type of the promise
void generatorSend(llvm::Value *self, llvm::Value *val, llvm::BasicBlock *block,
                   llvm::Type *outType);

/// Generates logic to tear down a generator.
/// @param self the generator
/// @param block the block to codegen into
void generatorDestroy(llvm::Value *self, llvm::BasicBlock *block);

/// Generates logic to yield from a generator.
/// @param val the value to yield, may be nullptr
/// @param block the block to codegen into
/// @param dst the block to jump to, may be nullptr if final yield
/// @param promise the promise's location
/// @param suspend the suspend block
/// @param cleanup the cleanup block
void generatorYield(llvm::Value *val, llvm::BasicBlock *block, llvm::BasicBlock *dst,
                    llvm::Value *promise, llvm::BasicBlock *suspend,
                    llvm::BasicBlock *cleanup);

/// Generates logic to yield into a generator.
/// @param ptr pointer to the variable to store into
/// @param block the block to codegen into
/// @param dst the block to jump to
/// @param promise the promise's location
/// @param suspend the suspend block
/// @param cleanup the cleanup block
void generatorYieldIn(llvm::Value *ptr, llvm::BasicBlock *block, llvm::BasicBlock *dst,
                      llvm::Value *promise, llvm::BasicBlock *suspend,
                      llvm::BasicBlock *cleanup);

llvm::StructType *getTypeInfoType(llvm::LLVMContext &ctx);
llvm::StructType *getPadType(llvm::LLVMContext &ctx);
llvm::StructType *getExcType(llvm::LLVMContext &ctx);

/// Compile the SIR module in the given context.
/// @param context the LLVM context
/// @param module the SIR module.
/// @return the LLVM module result
llvm::Module *compile(llvm::LLVMContext &context, std::shared_ptr<SIRModule> module);

} // namespace codegen
} // namespace ir
} // namespace seq