#pragma once

#include <memory>

#include "base.h"

namespace seq {
namespace ir {

class Operand;
class Pattern;

class Rvalue : public AttributeHolder<Rvalue> {};

class CallRValue : public Rvalue {
private:
  std::shared_ptr<Operand> func;
  std::shared_ptr<Operand> args;
};

class OperandRvalue : public Rvalue {
private:
  std::shared_ptr<Operand> operand;
};

class MatchRvalue : public Rvalue {
private:
  std::shared_ptr<Pattern> pattern;
  std::shared_ptr<Operand> operand;
};

class PipelineRvalue : public Rvalue {
private:
  std::vector<std::shared_ptr<Operand>> stages;
  std::vector<bool> parallel;
};

} // namespace ir
} // namespace seq