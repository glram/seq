#pragma once

#include <utility>
#include <vector>

#include "base.h"

#include "types/types.h"

namespace seq {
namespace ir {

namespace common {
class SIRVisitor;
}

class BasicBlock;
class Var;

/// SIR object representing a try catch.
class TryCatch : public AttributeHolder<TryCatch> {
private:
  /// globally shared try catch counter (try catches have unique ids).
  static int currentId;

  /// the try catch's children
  std::vector<std::shared_ptr<TryCatch>> children;
  /// the catch types
  std::vector<std::shared_ptr<types::Type>> catchTypes;
  /// the catch variable names
  std::vector<std::string> catchVarNames;
  /// the catch variables
  std::vector<std::shared_ptr<Var>> catchVars;
  /// the flag variable, used for routing finally's
  std::shared_ptr<Var> flagVar;
  /// the handler blocks
  std::vector<std::weak_ptr<BasicBlock>> catchBlocks;
  /// the finally block, nullptr if doesn't exist
  std::weak_ptr<BasicBlock> finallyBlock;
  /// the parent try catch
  std::weak_ptr<TryCatch> parent;
  /// the try catch's unique id
  int id;

public:
  TryCatch();

  /// Resets the globally shared try catch counter. Should only be used in testing.
  static void resetId();

  void accept(common::SIRVisitor &v);

  /// Adds a child to the try catch.
  /// @param child the child
  void addChild(std::shared_ptr<TryCatch> child) {
    children.push_back(child);
    child->parent = getShared();
  }

  /// @return the catch types
  std::vector<std::shared_ptr<types::Type>> getCatchTypes() { return catchTypes; }

  /// @return the handler blocks
  std::vector<std::weak_ptr<BasicBlock>> getCatchBlocks() { return catchBlocks; }

  /// Adds a handler.
  /// @param catchType the catch type, nullptr if a catch all
  /// @param name the variable name, empty if no variable
  /// @param handler the handler
  void addCatch(std::shared_ptr<types::Type> catchType, std::string name,
                std::weak_ptr<BasicBlock> handler);

  /// @return the finally block
  std::weak_ptr<BasicBlock> getFinallyBlock() { return finallyBlock; }

  /// Sets the finally block.
  /// @param finally the finally
  void setFinallyBlock(std::weak_ptr<BasicBlock> finally) {
    finallyBlock = std::move(finally);
  }

  /// @return the parent try catch
  std::weak_ptr<TryCatch> getParent() { return parent; }

  /// @return the try catch's id
  int getId() const { return id; }

  /// Returns the variable at a given index
  /// @param i the index
  /// @return the variable at index i
  std::shared_ptr<Var> getVar(int i) { return catchVars[i]; }

  /// @return the flag variable
  std::shared_ptr<Var> getFlagVar() { return flagVar; }

  /// Returns the path between the try catch and dst.
  /// @param dst the desired ending try catch, may be nullptr
  /// @return the path between this and dst, excluding dst
  std::vector<std::shared_ptr<TryCatch>> getPath(std::shared_ptr<TryCatch> dst);

  std::string referenceString() const override;
  std::string textRepresentation() const override;
};

} // namespace ir
} // namespace seq
