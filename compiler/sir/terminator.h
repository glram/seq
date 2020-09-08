#pragma once

#include "base.h"
#include <memory>
#include <string>
#include <utility>

namespace seq {
namespace ir {

class BasicBlock;
class Operand;
class Var;

class Terminator : public AttributeHolder<Terminator> {
public:
  virtual ~Terminator() = default;
  std::string referenceString() const override { return "terminator"; }
};

class JumpTerminator : public Terminator {
private:
  std::weak_ptr<BasicBlock> dst;

public:
  explicit JumpTerminator(std::weak_ptr<BasicBlock> dst) : dst(std::move(dst)){};

  std::weak_ptr<BasicBlock> getDst() { return dst; }

  std::string textRepresentation() const override;
};

class CondJumpTerminator : public Terminator {
private:
  std::weak_ptr<BasicBlock> tDst;
  std::weak_ptr<BasicBlock> fDst;
  std::shared_ptr<Operand> cond;

public:
  CondJumpTerminator(std::weak_ptr<BasicBlock> tDst, std::weak_ptr<BasicBlock> fDst,
                     std::shared_ptr<Operand> cond)
      : tDst(std::move(tDst)), fDst(std::move(fDst)), cond(std::move(cond)){};

  std::shared_ptr<Operand> getCond() { return cond; }
  std::weak_ptr<BasicBlock> getTDst() { return tDst; }
  std::weak_ptr<BasicBlock> getFDst() { return fDst; }

  std::string textRepresentation() const override;
};

class ReturnTerminator : public Terminator {
private:
  std::shared_ptr<Operand> operand;

public:
  explicit ReturnTerminator(std::shared_ptr<Operand> operand)
      : operand(std::move(operand)){};
  std::string textRepresentation() const override;
  std::shared_ptr<Operand> getOperand() { return operand; }
};

class YieldTerminator : public Terminator {
private:
  std::weak_ptr<BasicBlock> dst;
  std::shared_ptr<Operand> res;
  std::weak_ptr<Var> inVar;

public:
  YieldTerminator(std::weak_ptr<BasicBlock> dst, std::shared_ptr<Operand> result,
                  std::weak_ptr<Var> inVar)
      : dst(std::move(dst)), res(std::move(result)), inVar(std::move(inVar)) {}

  std::weak_ptr<BasicBlock> getDst() { return dst; }
  std::shared_ptr<Operand> getResult() { return res; }
  std::weak_ptr<Var> getInVar() { return inVar; }

  std::string textRepresentation() const override;
};

class ThrowTerminator : public Terminator {
private:
  std::shared_ptr<Operand> operand;

public:
  explicit ThrowTerminator(std::shared_ptr<Operand> operand)
      : operand(std::move(operand)){};

  std::shared_ptr<Operand> getOperand() { return operand; }

  std::string textRepresentation() const override;
};

} // namespace ir
} // namespace seq
