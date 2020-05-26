#pragma once

#include "base.h"
#include <memory>
#include <string>

namespace seq {
namespace ir {

class BasicBlock;
class Expression;
class Var;

class Terminator : public AttributeHolder {};

class JumpTerminator : public Terminator {
private:
  std::weak_ptr<BasicBlock> dst;

public:
  JumpTerminator(std::weak_ptr<BasicBlock> dst);
  std::weak_ptr<BasicBlock> getDst() const;

  std::string textRepresentation() const;
};

class CondJumpTerminator : public Terminator {
private:
  std::weak_ptr<BasicBlock> tDst;
  std::weak_ptr<BasicBlock> fDst;
  std::shared_ptr<Expression> cond;

public:
  CondJumpTerminator(std::weak_ptr<BasicBlock> tDst,
                     std::weak_ptr<BasicBlock> fDst,
                     std::shared_ptr<Expression> cond);

  std::shared_ptr<Expression> getCond() const;
  std::weak_ptr<BasicBlock> getTDst() const;
  std::weak_ptr<BasicBlock> getFDst() const;

  std::string textRepresentation() const;
};

class ReturnTerminator : public Terminator {
public:
  std::string textRepresentation() const;
};

class YieldTerminator : public Terminator {
private:
  std::weak_ptr<BasicBlock> dst;
  // TODO - reverse yield
  std::weak_ptr<Var> result;

public:
  YieldTerminator(std::weak_ptr<BasicBlock> dst, std::weak_ptr<Var> result);

  std::weak_ptr<BasicBlock> getDst() const;
  std::weak_ptr<Var> getResult() const;

  std::string textRepresentation() const;
};

class ThrowTerminator : public Terminator {
private:
  std::shared_ptr<Expression> expr;

public:
  ThrowTerminator(std::shared_ptr<Expression> expr);
  std::shared_ptr<Expression> getExpr() const;

  std::string textRepresentation() const;
};
} // namespace ir
} // namespace seq
