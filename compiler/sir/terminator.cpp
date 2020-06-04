#include <sstream>

#include "bblock.h"
#include "operand.h"
#include "terminator.h"
#include "var.h"

using namespace seq;
using namespace ir;

JumpTerminator::JumpTerminator(std::weak_ptr<BasicBlock> dst) : dst{dst} {}

std::weak_ptr<BasicBlock> JumpTerminator::getDst() const { return dst; }

std::string JumpTerminator::textRepresentation() const {
  std::stringstream stream;
  auto lockedDst = dst.lock();

  stream << "jmp " << lockedDst->referenceString() << "; " << attributeString();
  return stream.str();
}

CondJumpTerminator::CondJumpTerminator(std::weak_ptr<BasicBlock> tDst,
                                       std::weak_ptr<BasicBlock> fDst,
                                       std::shared_ptr<Operand> cond)
    : tDst{tDst}, fDst{fDst}, cond{cond} {}

std::shared_ptr<Operand> CondJumpTerminator::getCond() const { return cond; }

std::weak_ptr<BasicBlock> CondJumpTerminator::getTDst() const { return tDst; }

std::weak_ptr<BasicBlock> CondJumpTerminator::getFDst() const { return fDst; }

std::string CondJumpTerminator::textRepresentation() const {
  std::stringstream stream;
  auto lockedTDst = tDst.lock();
  auto lockedFDst = fDst.lock();

  stream << "condjmp (" << cond->textRepresentation() << ") "
         << lockedTDst->referenceString() << " "
         << lockedFDst->referenceString() << "; " << attributeString();

  return stream.str();
}

std::string ReturnTerminator::textRepresentation() const {
  std::stringstream stream;

  stream << "return";
  if (operand) {
    stream << " " << operand->textRepresentation();
  }
  stream << "; " << attributeString();
  return stream.str();
}

ReturnTerminator::ReturnTerminator(std::shared_ptr<Operand> operand)
    : operand{operand} {}

std::shared_ptr<Operand> ReturnTerminator::getOperand() const {
  return operand;
}

YieldTerminator::YieldTerminator(std::weak_ptr<BasicBlock> dst,
                                 std::shared_ptr<Operand> result,
                                 std::weak_ptr<Var> inVar)
    : dst{dst}, result{result}, inVar{invar} {}

std::weak_ptr<BasicBlock> YieldTerminator::getDst() const { return dst; }

std::weak_ptr<Var> YieldTerminator::getInVar() const { return inVar; }

std::string YieldTerminator::textRepresentation() const {
  std::stringstream stream;
  auto lockedDst = dst.lock();
  auto lockedInVar = inVar.lock();

  stream << "yield";
  if (result) {
    stream << " " << result->textRepresentation();
  }
  if (lockedInVar) {
    stream << "-> " << lockedInVar->referenceString();
  }

  stream << " " << lockedDst->referenceString() << "; " << attributeString();

  return stream.str();
}

std::shared_ptr<Operand> YieldTerminator::getResult() const {return result}

ThrowTerminator::ThrowTerminator(std::shared_ptr<Operand> operand)
    : operand{operand} {}

std::shared_ptr<Operand> ThrowTerminator::getOperand() const { return operand; }

std::string ThrowTerminator::textRepresentation() const {
  std::stringstream stream;
  stream << "throw (" << operand->textRepresentation() << ")"
         << attributeString();
  return stream.str();
}
