#pragma once

#include "base.h"
#include <memory>
#include <string>

namespace seq {
namespace ir {

class BasicBlock;
class Operand;
class Var;

class Terminator : public AttributeHolder<Terminator> {};

class JumpTerminator : public Terminator {
private:
  std::weak_ptr<BasicBlock> dst;

public:
  JumpTerminator(std::weak_ptr<BasicBlock> dst);
  std::weak_ptr<BasicBlock> getDst() const;

  std::string textRepresentation() const override;
};

class CondJumpTerminator : public Terminator {
private:
  std::weak_ptr<BasicBlock> tDst;
  std::weak_ptr<BasicBlock> fDst;
  std::shared_ptr<Operand> cond;

public:
  CondJumpTerminator(std::weak_ptr<BasicBlock> tDst,
                     std::weak_ptr<BasicBlock> fDst,
                     std::shared_ptr<Operand> cond);

  std::shared_ptr<Operand> getCond() const;
  std::weak_ptr<BasicBlock> getTDst() const;
  std::weak_ptr<BasicBlock> getFDst() const;

  std::string textRepresentation() const override;
};

class ReturnTerminator : public Terminator {
private:
  std::shared_ptr<Operand> operand;

public:
  explicit ReturnTerminator(std::shared_ptr<Operand> operand);
  std::string textRepresentation() const override;
  std::shared_ptr<Operand> getOperand() const;
};

class YieldTerminator : public Terminator {
private:
  std::weak_ptr<BasicBlock> dst;
  std::shared_ptr<Operand> result;
  // TODO - reverse yield
  std::weak_ptr<Var> inVar;

public:
  YieldTerminator(std::weak_ptr<BasicBlock> dst,
                  std::shared_ptr<Operand> result, std::weak_ptr<Var> inVar);

  std::weak_ptr<BasicBlock> getDst() const;
  std::shared_ptr<Operand> getResult() const;
  std::weak_ptr<Var> getInVar() const;

  std::string textRepresentation() const override;
};

class ThrowTerminator : public Terminator {
private:
  std::shared_ptr<Operand> operand;

public:
  ThrowTerminator(std::shared_ptr<Operand> operand);
  std::shared_ptr<Operand> getOperand() const;

  std::string textRepresentation() const override;
};
} // namespace ir
} // namespace seq
