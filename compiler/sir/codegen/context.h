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

struct TypeRealization;

/// Struct encapsulating a try catch's realization.
struct TryCatchRealization {
  /// the branch instruction to be added to the end of the try catch's finally
  llvm::SwitchInst *finallyBr = nullptr;
  /// the exception block
  llvm::BasicBlock *exceptionBlock = nullptr;
  /// the first block of the finally
  llvm::BasicBlock *finallyStart = nullptr;
  /// the handler blocks of the try catch
  std::vector<llvm::BasicBlock *> handlers;
  /// location to store the try catch's finally route flag
  llvm::Value *excFlag = nullptr;

  /// Stores a value into this try catch's finally route flag such that it jumps to the
  /// specified destination.
  /// @param dst the destination
  /// @param builder builder used to add the store instructions
  void storeDstValue(llvm::BasicBlock *dst, llvm::IRBuilder<> &builder) const;
};

/// Struct encapsulating LLVM objects necessary for function codegen.
struct CodegenFrame {
  /// map from SIR variable id to LLVM value
  std::unordered_map<int, llvm::Value *> varRealizations;
  /// map from SIR block id to LLVM block
  std::unordered_map<int, llvm::BasicBlock *> blockRealizations;
  /// map from SIR try catch id to realization
  std::unordered_map<int, std::shared_ptr<TryCatchRealization>> tryCatchRealizations;

  /// the current LLVM block being codegen'd into
  llvm::BasicBlock *curBlock = nullptr;
  /// the current SIR block being transformed
  std::shared_ptr<BasicBlock> curIRBlock;

  /// the current LLVM function
  llvm::Function *func = nullptr;
  /// the current SIR function
  std::shared_ptr<Func> irFunc;

  /// the realization of the return/yield type
  std::shared_ptr<TypeRealization> outReal;

  /// true if the function is a generator, false otherwise
  bool isGenerator = false;

  /// location to store exceptions
  llvm::Value *catchStore = nullptr;

  /// location to store the return value
  llvm::Value *rValPtr = nullptr;

  /// location for this coroutine's promise, or null if none
  llvm::Value *promise = nullptr;

  /// coroutine's handle, or null if none
  llvm::Value *handle = nullptr;

  /// coroutine cleanup block, or null if none
  llvm::BasicBlock *cleanup = nullptr;

  /// coroutine suspend block, or null if none
  llvm::BasicBlock *suspend = nullptr;

  /// coroutine exit block, or null if none. If not a coroutine, this is the return
  /// block
  llvm::BasicBlock *exit = nullptr;
};

/// Struct encapsulating a type's realization.
struct TypeRealization {
public:
  /// the SIR type
  std::shared_ptr<seq::ir::types::Type> irType;
  /// the LLVM type
  llvm::Type *llvmType;

  using Fields = std::unordered_map<std::string, int>;
  /// a map from field names to indices
  Fields fields;

  using CustomGetterFunc =
      std::function<llvm::Value *(llvm::Value *, llvm::IRBuilder<> &)>;
  using CustomGetters = std::unordered_map<std::string, CustomGetterFunc>;
  /// a map from field names to loaders
  CustomGetters customGetters;

  using MemberPointerFunc =
      std::function<llvm::Value *(llvm::Value *, int, llvm::IRBuilder<> &)>;
  /// a function to get an LLVM pointer value to a particular member of the type.
  MemberPointerFunc memberPointerFunc;

  using DefaultBuilder = std::function<llvm::Value *(llvm::IRBuilder<> &)>;
  /// a function to get a default value for the type
  DefaultBuilder dfltBuilder;

  using InlineMagicBuilder =
      std::function<llvm::Value *(std::vector<llvm::Value *>, llvm::IRBuilder<> &)>;
  using InlineMagics = std::unordered_map<std::string, InlineMagicBuilder>;
  /// functions to build inline magic calls
  InlineMagics inlineMagicFuncs;

  using NonInlineMagicBuilder = std::function<void(llvm::Function *)>;
  using NonInlineMagics = std::unordered_map<std::string, NonInlineMagicBuilder>;
  /// functions to build non-inline magic calls
  NonInlineMagics nonInlineMagicFuncs;

  /// the signature corresponding to the default __new__ function
  std::string maker;

  using CustomLoader = std::function<llvm::Value *(llvm::Value *, llvm::IRBuilder<> &)>;
  /// a function to load a LLVM pointer value to the type
  CustomLoader customLoader;

public:
  /// Construct a type realization.
  /// @param irType the SIR type
  /// @param llvmType the LLVM type
  /// @param dfltBuilder a function to build a default value for the type
  /// @param inlineMagicFuncs a map of functions to build inline magic calls
  /// @param newSig the signature of the default __new__ function
  /// @param nonInlineMagicFuncs a map of functions to build non-inline magic calls
  /// @param fields a map from fields to indices in the LLVM struct
  /// @param customGetters a map from fields to functions to extract the relevant member
  /// of the type
  /// @param memberPointerFunc a function to get a LLVM pointer value to the relevant
  /// member of the type
  /// @param customLoader a function to load a value from a pointer
  TypeRealization(std::shared_ptr<seq::ir::types::Type> irType, llvm::Type *llvmType,
                  DefaultBuilder dfltBuilder, InlineMagics inlineMagicFuncs = {},
                  const std::string &newSig = "",
                  NonInlineMagics nonInlineMagicFuncs = {}, Fields fields = {},
                  CustomGetters customGetters = {},
                  MemberPointerFunc memberPointerFunc = nullptr,
                  CustomLoader customLoader = nullptr)
      : irType(std::move(irType)), llvmType(llvmType), fields(std::move(fields)),
        customGetters(std::move(customGetters)),
        memberPointerFunc(std::move(memberPointerFunc)),
        dfltBuilder(std::move(dfltBuilder)),
        inlineMagicFuncs(std::move(inlineMagicFuncs)),
        nonInlineMagicFuncs(std::move(nonInlineMagicFuncs)), maker(newSig),
        customLoader(std::move(customLoader)) {}

  /// Construct a type realization.
  /// @param irType the SIR type
  /// @param llvmType the LLVM type
  TypeRealization(std::shared_ptr<seq::ir::types::Type> irType, llvm::Type *llvmType)
      : TypeRealization(std::move(irType), llvmType, nullptr) {}

  /// Get a the value of a member from a value.
  /// @param self a value
  /// @param field the relevant field
  /// @param builder a builder to codegen with
  /// @return the extracted value
  llvm::Value *extractMember(llvm::Value *self, const std::string &field,
                             llvm::IRBuilder<> &builder) const;

  /// Get a LLVM pointer value to a member from a LLVM pointer value.
  /// @param ptr a LLVM pointer value
  /// @param field the relevant field
  /// @param builder a builder to codegen with
  /// @return a LLVM pointer value the relevant field
  llvm::Value *getMemberPointer(llvm::Value *ptr, const std::string &field,
                                llvm::IRBuilder<> &builder) const;

  /// Gets a default value.
  /// @param builder a builder to codegen with
  /// @return a default value
  llvm::Value *getDefaultValue(llvm::IRBuilder<> &builder) const;

  /// Get an undefined value.
  /// @return an undefined value
  llvm::Value *getUndefValue() const;

  /// Calls a magic function inline.
  /// @param sig the magic signature
  /// @param args the arguments to the magic call
  /// @param builder a builder to codegen with
  /// @return the result of the magic call
  llvm::Value *callMagic(const std::string &sig, std::vector<llvm::Value *> args,
                         llvm::IRBuilder<> &builder);

  /// Gets the builder for a magic function.
  /// @param sig the magic signature
  /// @return a function that builds the magic into an LLVM function
  NonInlineMagicBuilder getMagicBuilder(const std::string &sig) const;

  /// Creates a new value.
  /// @param args the arguments to the new call
  /// @param builder a builder to codegen with
  /// @return the new value
  llvm::Value *makeNew(std::vector<llvm::Value *> args,
                       llvm::IRBuilder<> &builder) const;

  /// Loads a value from a LLVM pointer value.
  /// @param ptr the LLVM pointer value
  /// @param builder a builder to codegen with
  /// @return the loaded value
  llvm::Value *load(llvm::Value *ptr, llvm::IRBuilder<> &builder) const;

  /// Allocates an array.
  /// @param count the length
  /// @param builder a builder to codegen with
  /// @param stack true if allocating on the stack, false otherwise
  /// @return a LLVM pointer value to the first element of the array
  llvm::Value *alloc(llvm::Value *count, llvm::IRBuilder<> &builder,
                     bool stack = false) const;

  /// Allocates an array with 1 element.
  /// @param builder a builder to codegen with
  /// @param stack true if allocating on the stack, false otherwise
  /// @return a LLVM pointer value to the first element of the array
  llvm::Value *alloc(llvm::IRBuilder<> &builder, bool stack = false) const;
};

/// Struct encapsulating a builtin stub.
struct BuiltinStub {
  /// the SIR function
  std::shared_ptr<Func> sirFunc;
  /// the LLVM function, may be stubbed
  llvm::Function *func;
  /// true if the builtin has been codegen'd, false otherwise
  bool doneGen = false;

  /// Constructs a builtin realization.
  /// @param sirFunc the SIR function
  /// @param func the LLVM function
  BuiltinStub(std::shared_ptr<Func> sirFunc, llvm::Function *func)
      : sirFunc(std::move(sirFunc)), func(func) {}
};

/// Class encapsulating codegen metadata.
class Context : public seq::ir::common::IRContext<CodegenFrame> {
private:
  /// the LLVM module
  llvm::Module *module;
  /// a map from SIR type id to realizations
  std::unordered_map<int, std::shared_ptr<TypeRealization>> typeRealizations;
  /// a map from builtin names to realizations
  std::unordered_map<std::string, std::shared_ptr<BuiltinStub>> builtinStubs;

  void initTypeRealizations();

public:
  /// Constructs an empty context.
  /// @param module the LLVM module
  explicit Context(llvm::Module *module) : module(module) { initTypeRealizations(); }

  /// Registers a type realization globally.
  /// @param sirType the SIR type
  /// @param t the type realization
  void registerType(std::shared_ptr<types::Type> sirType,
                    std::shared_ptr<TypeRealization> t);

  /// Gets the realization of an SIR type.
  /// @param sirType the SIR type
  /// @return the realization if it exists, nullptr otherwise
  std::shared_ptr<TypeRealization>
  getTypeRealization(std::shared_ptr<types::Type> sirType);

  /// Stubs a builtin globally.
  /// @param sirFunc the SIR function.
  /// @param stub the stub
  void stubBuiltin(std::shared_ptr<Func> sirFunc, std::shared_ptr<BuiltinStub> stub);

  /// Gets a builtin stub.
  /// @param name the builtin's name
  /// @return the stub if it exists, nullptr otherwise
  std::shared_ptr<BuiltinStub> getBuiltinStub(const std::string &name);

  /// Registers a try catch realization in the current frame.
  /// @param tc the try catch
  /// @param real the try catch realization
  void registerTryCatch(std::shared_ptr<TryCatch> tc,
                        std::shared_ptr<TryCatchRealization> real);

  /// Gets the realization of a try catch.
  /// @param tc the try catch
  /// @return the realization if it exists, nullptr otherwise
  std::shared_ptr<TryCatchRealization>
  getTryCatchRealization(std::shared_ptr<TryCatch> tc);

  /// Registers a variable in the current frame.
  /// @param sirVar the SIR variable
  /// @param val the LLVM pointer value
  void registerVar(std::shared_ptr<Var> sirVar, llvm::Value *val);

  /// Gets the LLVM pointer value of a SIR variable.
  /// @param sirVar the SIR variable
  /// @return the LLVM pointer value if it exists, nullptr otherwise.
  llvm::Value *getVar(std::shared_ptr<Var> sirVar);

  /// Registers a block in the current frame.
  /// @param sirBlock the SIR block
  /// @param block the LLVM block
  void registerBlock(std::shared_ptr<BasicBlock> sirBlock, llvm::BasicBlock *block);

  /// Gets the LLVM block of a SIR block.
  /// @param sirBlock the SIR block
  /// @return the LLVM block if it exists, nullptr otherwise
  llvm::BasicBlock *getBlock(std::shared_ptr<BasicBlock> sirBlock);

  /// @return the LLVM module
  llvm::Module *getModule() { return module; }

  /// @return the LLVM context
  llvm::LLVMContext &getLLVMContext() { return module->getContext(); }

  llvm::Value *codegenStr(llvm::Value *self, const std::string &name,
                          llvm::BasicBlock *block);
};

} // namespace codegen
} // namespace ir
} // namespace seq
