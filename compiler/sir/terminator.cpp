#include <sstream>

#include "bblock.h"
#include "expr.h"
#include "terminator.h"
#include "var.h"

using namespace seq;
using namespace ir;

JumpTerminator::JumpTerminator(std::weak_ptr<BasicBlock> dst) : dst{dst} {}

std::weak_ptr<BasicBlock> JumpTerminator::getDst() const { return dst; }

std::string JumpTerminator::textRepresentation() const {
  std::stringstream stream;
  auto lockedDst = dst.lock();

  stream << Terminator::textRepresentation() << " jmp "
         << lockedDst->referenceString();
  return stream.str();
}

CondJumpTerminator::CondJumpTerminator(std::weak_ptr<BasicBlock> tDst,
                                       std::weak_ptr<BasicBlock> fDst,
                                       std::shared_ptr<Expression> cond)
    : tDst{tDst}, fDst{fDst}, cond{cond} {}

std::shared_ptr<Expression> CondJumpTerminator::getCond() const { return cond; }

std::weak_ptr<BasicBlock> CondJumpTerminator::getTDst() const { return tDst; }

std::weak_ptr<BasicBlock> CondJumpTerminator::getFDst() const { return fDst; }

std::string CondJumpTerminator::textRepresentation() const {
  std::stringstream stream;
  auto lockedTDst = tDst.lock();
  auto lockedFDst = fDst.lock();

  stream << Terminator::textRepresentation() << " condjmp ("
         << cond->textRepresentation() << ") " << lockedTDst->referenceString()
         << " " << lockedFDst->referenceString();

  return stream.str();
}

std::string ReturnTerminator::textRepresentation() const {
  std::stringstream stream;

  stream << Terminator::textRepresentation() << " return";
  return stream.str();
}

YieldTerminator::YieldTerminator(std::weak_ptr<BasicBlock> dst,
                                 std::weak_ptr<Var> result)
    : dst{dst}, result{result} {}

std::weak_ptr<BasicBlock> YieldTerminator::getDst() const { return dst; }

std::weak_ptr<Var> YieldTerminator::getResult() const { return result; }

std::string YieldTerminator::textRepresentation() const {
  std::stringstream stream;
  auto lockedDst = dst.lock();
  auto lockedRes = result.lock();

  stream << Terminator::textRepresentation() << " yield";
  if (lockedRes) {
    stream << "-> " << lockedRes->referenceString();
  }

  stream << " " << lockedDst->referenceString();

  return stream.str();
}

ThrowTerminator::ThrowTerminator(std::shared_ptr<Expression> expr)
    : expr{expr} {}

std::shared_ptr<Expression> ThrowTerminator::getExpr() const { return expr; }
std::string ThrowTerminator::textRepresentation() const {
  std::stringstream stream;
  stream << Terminator::textRepresentation() << " throw ("
         << expr->textRepresentation() << ")";
  return stream.str();
}
