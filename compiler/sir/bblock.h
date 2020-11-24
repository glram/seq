#pragma once

#include <memory>
#include <string>
#include <vector>

#include "base.h"

namespace seq {
namespace ir {

namespace common {
class SIRVisitor;
}

class Instr;
class Terminator;
class TryCatch;

/// SIR basic block, equivalent (but not directly corresponding to) an LLVM basic block.
class BasicBlock : public AttributeHolder<BasicBlock> {
private:
  /// globally shared block counter (blocks have unique ids).
  static int currentId;

  /// the block's id
  int id;

  /// the block's name
  std::string name;

  /// vector of instructions
  std::vector<std::shared_ptr<Instr>> instructions;

  /// terminator (should not be nullptr)
  std::shared_ptr<Terminator> terminator;


  /// the block's try catch
  std::shared_ptr<TryCatch> tc;

  /// true if the block is a try catch handler
  bool isCatch;

public:
  /// Constructs an SIR basic block.
  /// @param tc the block's try catch
  /// @param isCatchClause true if the block is a handler.
  explicit BasicBlock(std::string name = "", std::shared_ptr<TryCatch> tc = nullptr,
                      bool isCatchClause = false)
      : id(currentId++), name(std::move(name)), tc(std::move(tc)), isCatch(isCatchClause) {}

  void accept(common::SIRVisitor &v);

  /// Resets the globally shared block counter. Should only be used in testing.
  static void resetId();

  /// Adds an instruction to the end of the block.
  /// @param instruction the instruction
  void append(std::shared_ptr<Instr> instruction) {
    instructions.push_back(std::move(instruction));
  }

  /// @return the vector of instructions
  std::vector<std::shared_ptr<Instr>> getInstructions() { return instructions; }

  /// Sets the terminator of the block.
  /// @param t the terminator
  void setTerminator(std::shared_ptr<Terminator> t) { terminator = std::move(t); }

  /// @return the block's terminator
  std::shared_ptr<Terminator> getTerminator() { return terminator; }

  /// @return the block's id
  int getId() const { return id; }

  /// Sets the block's try catch.
  /// @param newTc the new try catch
  /// @param isCatchClause true if the block is a handler, false otherwise
  void setTryCatch(std::shared_ptr<TryCatch> newTc, bool isCatchClause = false) {
    tc = std::move(newTc);
    isCatch = isCatchClause;
  }

  /// @return the block's try catch
  std::shared_ptr<TryCatch> getTryCatch() { return tc; }

  /// @return the try catch responsible for handling exceptions in the block.
  std::shared_ptr<TryCatch> getHandlerTryCatch();

  /// @return the try catch containing the finally that exiting jumps must visit.
  std::shared_ptr<TryCatch> getFinallyTryCatch();

  /// @return true if the block is a handler, false otherwise
  bool isCatchClause() const { return isCatch; }

  std::string referenceString() const override;
  std::string textRepresentation() const override;
};

} // namespace ir
} // namespace seq
