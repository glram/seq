#include "instr.h"

#include "constant.h"
#include "module.h"
#include "util/iterators.h"

namespace seq {
namespace ir {

const char Instr::NodeId = 0;

const char AssignInstr::NodeId = 0;

std::ostream &AssignInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("store({}, {})"), lhs->referenceString(), *rhs);
  return os;
}

Value *AssignInstr::doClone() const {
  return getModule()->Nrs<AssignInstr>(getSrcInfo(), lhs, rhs->clone(), getName());
}

const char ExtractInstr::NodeId = 0;

types::Type *ExtractInstr::getType() const {
  auto *memberedType = val->getType()->as<types::MemberedType>();
  assert(memberedType);
  return memberedType->getMemberType(field);
}

Value *ExtractInstr::doClone() const {
  return getModule()->Nrs<ExtractInstr>(getSrcInfo(), val->clone(), field, getName());
}

std::ostream &ExtractInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("extract({}, \"{}\")"), *val, field);
  return os;
}

const char InsertInstr::NodeId = 0;

std::ostream &InsertInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("insert({}, \"{}\", {})"), *lhs, field, *rhs);
  return os;
}

Value *InsertInstr::doClone() const {
  return getModule()->Nrs<InsertInstr>(getSrcInfo(), lhs->clone(), field, rhs->clone(),
                                       getName());
}

const char CallInstr::NodeId = 0;

types::Type *CallInstr::getType() const {
  if (auto *intrinsic = cast<IntrinsicConstant>(func))
    return intrinsic->getReturnType();

  auto *funcType = func->getType()->as<types::FuncType>();
  assert(funcType);
  return funcType->getReturnType();
}

std::vector<Value *> CallInstr::getChildren() const {
  std::vector<Value *> ret;
  for (auto &v : *this)
    ret.push_back(v.get());
  ret.push_back(func.get());
  return ret;
}

std::ostream &CallInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("call({}, {})"), *func,
             fmt::join(util::dereference_adaptor(args.begin()),
                       util::dereference_adaptor(args.end()), ", "));
  return os;
}

Value *CallInstr::doClone() const {
  std::vector<ValuePtr> clonedArgs;
  for (const auto &arg : *this)
    clonedArgs.push_back(arg->clone());
  return getModule()->Nrs<CallInstr>(getSrcInfo(), func->clone(), std::move(clonedArgs),
                                     getName());
}

const char StackAllocInstr::NodeId = 0;

std::ostream &StackAllocInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("stack_alloc({}, {})"), arrayType->referenceString(),
             *count);
  return os;
}

Value *StackAllocInstr::doClone() const {
  return getModule()->Nrs<StackAllocInstr>(getSrcInfo(), arrayType, count->clone(),
                                           getName());
}

const char YieldInInstr::NodeId = 0;

std::ostream &YieldInInstr::doFormat(std::ostream &os) const {
  return os << "yield_in()";
}

Value *YieldInInstr::doClone() const {
  return getModule()->Nrs<YieldInInstr>(getSrcInfo(), type, suspend, getName());
}

const char TernaryInstr::NodeId = 0;

std::ostream &TernaryInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("ternary({}, {}, {})"), *cond, trueValue, *falseValue);
  return os;
}

Value *TernaryInstr::doClone() const {
  return getModule()->Nrs<TernaryInstr>(getSrcInfo(), cond->clone(), trueValue->clone(),
                                        falseValue->clone(), getName());
}

const char ControlFlowInstr::NodeId = 0;

const char UnconditionalControlFlowInstr::NodeId = 0;

std::vector<Flow *> UnconditionalControlFlowInstr::getTargets() const {
  if (!target)
    return {};
  else if (auto *proxy = cast<ValueProxy>(target)) {
    auto *flow = cast<Flow>(proxy->getValue());
    assert(flow);
    return {flow};
  } else if (auto *flow = cast<Flow>(target))
    return {flow};

  assert(false);
}

const char BreakInstr::NodeId = 0;

std::ostream &BreakInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("break({})"), *getTarget());
  return os;
}

Value *BreakInstr::doClone() const {
  return getModule()->Nrs<BreakInstr>(getSrcInfo(), getTarget()->clone(), getName());
}

const char ContinueInstr::NodeId = 0;

std::ostream &ContinueInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("continue({})"), *getTarget());
  return os;
}

Value *ContinueInstr::doClone() const {
  return getModule()->Nrs<ContinueInstr>(getSrcInfo(), getTarget()->clone(), getName());
}

const char ReturnInstr::NodeId = 0;

std::ostream &ReturnInstr::doFormat(std::ostream &os) const {
  if (value) {
    fmt::print(os, FMT_STRING("return({})"), *value);
  } else {
    os << "return()";
  }
  return os;
}

Value *ReturnInstr::doClone() const {
  return getModule()->Nrs<ReturnInstr>(getSrcInfo(), value ? value->clone() : nullptr,
                                       getName());
}

const char BranchInstr::NodeId = 0;

std::ostream &BranchInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("jmp({})"), *getTarget());
  return os;
}

Value *BranchInstr::doClone() const {
  return getModule()->Nrs<BranchInstr>(getSrcInfo(), getTarget()->clone(), getName());
}

std::ostream &operator<<(std::ostream &os, const CondBranchInstr::Target &t) {
  fmt::print(os, FMT_STRING("({}, {})"),
             t.cond ? fmt::format(FMT_STRING("{}"), *t.cond) : "true",
             *t.dst);
  return os;
}

const char CondBranchInstr::NodeId = 0;

std::vector<Flow *> CondBranchInstr::getTargets() const {
  std::vector<Flow *> ret;
  for (auto &t : targets) {
    auto &target = t.dst;
    if (auto *proxy = cast<ValueProxy>(target)) {
      auto *flow = cast<Flow>(proxy->getValue());
      assert(flow);
      return {flow};
    } else if (auto *flow = cast<Flow>(target))
      return {flow};
    else
      assert(false);
  }
  return ret;
}

std::vector<Value *> CondBranchInstr::getChildren() const {
  std::vector<Value *> ret;
  for (auto &c : *this) {
    if (c.cond)
      ret.push_back(c.cond.get());
    ret.push_back(c.dst.get());
  }
  return ret;
}

std::ostream &CondBranchInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("cond_jmp({})"),
             fmt::join(targets.begin(), targets.end(), ", "));
  return os;
}

Value *CondBranchInstr::doClone() const {
  std::vector<Target> newTargets;
  for (auto &t : targets)
    newTargets.emplace_back(t.dst->clone(), t.cond->clone());

  return getModule()->Nrs<CondBranchInstr>(getSrcInfo(), std::move(newTargets),
                                           getName());
}

const char YieldInstr::NodeId = 0;

std::ostream &YieldInstr::doFormat(std::ostream &os) const {
  if (value) {
    fmt::print(os, FMT_STRING("yield({})"), *value);
  } else {
    os << "yield()";
  }
  return os;
}

Value *YieldInstr::doClone() const {
  return getModule()->Nrs<YieldInstr>(getSrcInfo(), value ? value->clone() : nullptr,
                                      getName());
}

const char ThrowInstr::NodeId = 0;

std::ostream &ThrowInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("throw({})"), *value);
  return os;
}

Value *ThrowInstr::doClone() const {
  return getModule()->Nrs<ThrowInstr>(getSrcInfo(), value->clone(), getName());
}

const char FlowInstr::NodeId = 0;

std::ostream &FlowInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("inline_flow({}, {})"), *flow, *val);
  return os;
}

Value *FlowInstr::doClone() const {
  return getModule()->Nrs<FlowInstr>(getSrcInfo(), flow->clone(), val->clone(),
                                     getName());
}

std::ostream &operator<<(std::ostream &os, const PhiInstr::Predecessor &p) {
  fmt::print(os, FMT_STRING("({}, {})"), *p.pred, *p.val);
  return os;
}

const char PhiInstr::NodeId = 0;

std::ostream &PhiInstr::doFormat(std::ostream &os) const {
  fmt::print(os, FMT_STRING("phi({})"),
             fmt::join(predecessors.begin(), predecessors.end(), ", "));
  return os;
}

Value *PhiInstr::doClone() const {
  std::vector<Predecessor> newPreds;
  for (auto &t : predecessors)
    newPreds.emplace_back(t.pred->clone(), t.val->clone());

  return getModule()->Nrs<PhiInstr>(getSrcInfo(), std::move(newPreds), getName());
}

std::vector<Value *> PhiInstr::getChildren() const {
  std::vector<Value *> ret;
  for (auto &c : *this) {
    ret.push_back(c.pred.get());
    ret.push_back(c.val.get());
  }
  return ret;
}

} // namespace ir
} // namespace seq
