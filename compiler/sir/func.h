#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "types/types.h"
#include "var.h"

namespace seq {
namespace ir {

namespace common {
class SIRVisitor;
}

class SIRModule;
class BasicBlock;
class TryCatch;

/// SIR function
class Func : public Var {
private:
  /// vector of variables representing arguments to the function
  std::vector<std::shared_ptr<Var>> argVars;
  /// vector of names of the arguments
  std::vector<std::string> argNames;

  /// vector of variables defined and used within the function
  std::vector<std::shared_ptr<Var>> vars;
  /// vector of blocks in the function
  std::vector<std::shared_ptr<BasicBlock>> blocks;

  /// the module in which the function is defined
  std::weak_ptr<SIRModule> module;
  /// the function enclosing this one, may be nullptr
  std::shared_ptr<Func> enclosing;

  /// the top level try catches contained in the function
  std::vector<std::shared_ptr<TryCatch>> tryCatches;

  /// true if external, false otherwise
  bool external;
  /// true if the function is a generator, false otherwise
  bool generator;

  /// true if the function is internal, false otherwise
  bool internal;
  /// true if the function is builtin, false otherwise
  bool builtin;
  /// parent type of the function if it is magic
  std::shared_ptr<types::Type> parent;
  /// unmangled name of the function
  std::string unmangledName;

public:
  /// Constructs an SIR function.
  /// @param name the function's name
  /// @param argNames the function's argument names
  /// @param type the function's type
  Func(std::string name, std::vector<std::string> argNames,
       std::shared_ptr<types::Type> type);

  /// Constructs an SIR function with no args and void return type.
  /// @param name the function's name
  explicit Func(std::string name)
      : Func(std::move(name), {}, types::kNoArgVoidFuncType) {}

  void accept(common::SIRVisitor &v) override;

  /// @return the function's argument names.
  std::vector<std::string> getArgNames() const { return argNames; }

  /// Re-initializes the function with a new type and names.
  /// @param type the function's new type
  /// @param names the function's new argument names
  void realize(std::shared_ptr<types::FuncType> type, std::vector<std::string> names);

  /// @return the function's argument variables
  std::vector<std::shared_ptr<Var>> getArgVars() { return argVars; }

  /// @param name the argument name
  /// @return the corresponding variable
  std::shared_ptr<Var> getArgVar(const std::string &name);

  /// Adds a variable to the function.
  /// @param var the variable
  void addVar(std::shared_ptr<Var> var) { vars.push_back(std::move(var)); }

  /// @return all non-argument variables used in the function.
  std::vector<std::shared_ptr<Var>> getVars() { return vars; }

  /// @return all blocks in the function
  std::vector<std::shared_ptr<BasicBlock>> getBlocks() { return blocks; }

  /// Adds a block to the function.
  /// @param block the new block
  void addBlock(std::shared_ptr<BasicBlock> block) {
    blocks.push_back(std::move(block));
  }

  /// Sets the function's enclosing function.
  /// @param f the parent function
  void setEnclosingFunc(std::shared_ptr<Func> f) { enclosing = std::move(f); }

  /// @return the parent function if exists, otherwise nullptr
  std::shared_ptr<Func> getEnclosingFunc() { return enclosing; }

  /// Adds a try catch to the function.
  /// @param tc the try catch
  void addTryCatch(std::shared_ptr<TryCatch> tc) {
    tryCatches.push_back(std::move(tc));
  }

  /// @return the function's top level try catches
  std::vector<std::shared_ptr<TryCatch>> getTryCatches() { return tryCatches; }

  /// Makes the function external.
  void setExternal() { external = true; }

  /// @return true if the function is external, false otherwise
  bool isExternal() const { return external; }

  /// Makes the function a generator.
  void setGenerator() { generator = true; }

  /// @return true if the function is a generator, false otherwise
  bool isGenerator() const { return generator; }

  /// Makes the function internal.
  /// @param p the function's parent type
  /// @param n the function's unmangled name
  void setInternal(std::shared_ptr<types::Type> p, std::string n) {
    internal = true;
    parent = std::move(p);
    unmangledName = std::move(n);
  }

  /// @return true if the function is internal, false otherwise
  bool isInternal() const { return internal; }

  /// Makes the function builtin.
  /// @param n the function's unmangled name
  void setBuiltin(std::string n) {
    builtin = true;
    unmangledName = std::move(n);
  }

  /// @return true if the function is builtin, false otherwise
  bool isBuiltin() const { return builtin; }

  /// @return the function's parent type if it exists, nullptr otherwise
  std::shared_ptr<types::Type> getParentType() { return parent; }

  /// @return the function's unmangled name if it exists, "" otherwise
  std::string getUnmangledName() const { return unmangledName; }

  std::string referenceString() const override;
  std::string textRepresentation() const override;

  bool isFunc() const override { return true; }
};

} // namespace ir
} // namespace seq
