#pragma once

#include <memory>
#include <string>
#include <utility>

#include "base.h"

namespace seq {
namespace ir {

namespace common {
class SIRVisitor;
}

class BasicBlock;
class Operand;
class Var;

/// SIR object representing the the terminator of a basic block.
class Terminator : public AttributeHolder<Terminator> {
public:
  virtual ~Terminator() = default;

  virtual void accept(common::SIRVisitor &v);

  std::string referenceString() const override { return "terminator"; }
};

/// Terminator representing an unconditional branch.
class JumpTerminator : public Terminator {
private:
  /// the destination
  std::weak_ptr<BasicBlock> dst;

public:
  /// Constructs a jump terminator.
  /// @param dst the destination
  explicit JumpTerminator(std::weak_ptr<BasicBlock> dst) : dst(std::move(dst)){};

  void accept(common::SIRVisitor &v) override;

  /// @return the destination
  std::weak_ptr<BasicBlock> getDst() { return dst; }

  std::string textRepresentation() const override;
};

/// Terminator representing a conditional jump.
class CondJumpTerminator : public Terminator {
private:
  /// the true destination
  std::weak_ptr<BasicBlock> tDst;
  /// the false destination
  std::weak_ptr<BasicBlock> fDst;
  /// the condition
  std::shared_ptr<Operand> cond;

public:
  /// Constructs a conditional jump.
  /// @param tDst the true destination
  /// @param fDst the false destination
  /// @param cond the condition
  CondJumpTerminator(std::weak_ptr<BasicBlock> tDst, std::weak_ptr<BasicBlock> fDst,
                     std::shared_ptr<Operand> cond)
      : tDst(std::move(tDst)), fDst(std::move(fDst)), cond(std::move(cond)){};

  void accept(common::SIRVisitor &v) override;

  /// @return the condition
  std::shared_ptr<Operand> getCond() { return cond; }

  /// @return the true destination
  std::weak_ptr<BasicBlock> getTDst() { return tDst; }

  /// @return the false destination
  std::weak_ptr<BasicBlock> getFDst() { return fDst; }

  std::string textRepresentation() const override;
};

/// Terminator representing a return from a function. In a generator, this represents
/// the final yield.
class ReturnTerminator : public Terminator {
private:
  /// the return value, nullptr if void
  std::shared_ptr<Operand> operand;

public:
  /// Constructs a return terminator.
  /// @param operand the return value, nullptr if void
  explicit ReturnTerminator(std::shared_ptr<Operand> operand = nullptr)
      : operand(std::move(operand)){};

  void accept(common::SIRVisitor &v) override;

  /// @return the return value
  std::shared_ptr<Operand> getOperand() { return operand; }

  std::string textRepresentation() const override;
};

/// Terminator representing a yield, may be in or out.
class YieldTerminator : public Terminator {
private:
  /// the next block
  std::weak_ptr<BasicBlock> dst;
  /// the result to yield out
  std::shared_ptr<Operand> res;
  /// the value to yield in
  std::shared_ptr<Var> inVar;

  /// true if yielding in, false otherwise
  bool yieldIn;

public:
  /// Constructs a yield out terminator.
  /// @param dst the next block
  /// @param result the result to yield out
  YieldTerminator(std::weak_ptr<BasicBlock> dst, std::shared_ptr<Operand> result)
      : dst(std::move(dst)), res(std::move(result)), yieldIn(false) {}

  /// Constructs a yield into terminator.
  /// @param dst the next block
  /// @param inVar the var to yield into
  YieldTerminator(std::weak_ptr<BasicBlock> dst, std::shared_ptr<Var> inVar)
      : dst(std::move(dst)), inVar(std::move(inVar)), yieldIn(true) {}

  void accept(common::SIRVisitor &v) override;

  /// @return true if the terminator is yield in, false otherwise
  bool isYieldIn() const { return yieldIn; }

  /// @return the next block
  std::weak_ptr<BasicBlock> getDst() { return dst; }

  /// @return the yield out result
  std::shared_ptr<Operand> getResult() { return res; }

  /// @return the variable to yield into
  std::shared_ptr<Var> getInVar() { return inVar; }

  std::string textRepresentation() const override;
};

/// Terminator representing throwing an exception.
class ThrowTerminator : public Terminator {
private:
  /// the exception
  std::shared_ptr<Operand> operand;

public:
  /// Constructs a throw terminator.
  /// @param operand the exception
  explicit ThrowTerminator(std::shared_ptr<Operand> operand)
      : operand(std::move(operand)){};

  void accept(common::SIRVisitor &v) override;

  /// @return the exception
  std::shared_ptr<Operand> getOperand() { return operand; }

  std::string textRepresentation() const override;
};

/// Terminator representing an assertion.
class AssertTerminator : public Terminator {
private:
  /// the operand being asserted
  std::shared_ptr<Operand> operand;
  /// the ok destination
  std::weak_ptr<BasicBlock> dst;
  /// the message, may be empty
  std::string msg;

public:
  /// Constructs an assert terminator.
  /// @param operand the operand being asserted
  /// @param dst the ok destination
  /// @param msg the message
  explicit AssertTerminator(std::shared_ptr<Operand> operand,
                            std::weak_ptr<BasicBlock> dst, std::string msg = "")
      : operand(std::move(operand)), dst(std::move(dst)), msg(std::move(msg)){};

  void accept(common::SIRVisitor &v) override;

  /// @return the ok destination
  std::weak_ptr<BasicBlock> getDst() { return dst; }

  /// @return the operand being asserted
  std::shared_ptr<Operand> getOperand() { return operand; }

  /// @return the message
  std::string getMsg() const { return msg; }

  std::string textRepresentation() const override;
};

/// Special terminator used only in finally blocks.
class FinallyTerminator : public Terminator {
private:
  /// the associated try catch
  std::shared_ptr<TryCatch> tc;

public:
  /// Constructs a finally terminator.
  /// @param tc the associated try catch
  explicit FinallyTerminator(std::shared_ptr<TryCatch> tc) : tc(std::move(tc)) {}

  void accept(common::SIRVisitor &v) override;

  /// @return the associated try catch
  std::shared_ptr<TryCatch> getTryCatch() { return tc; }

  std::string textRepresentation() const override;
};

} // namespace ir
} // namespace seq
