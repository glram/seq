#pragma once

#include <memory>
#include <string>

#include "flow.h"
#include "types/types.h"
#include "value.h"
#include "var.h"

namespace seq {
namespace ir {

/// SIR object representing an "instruction," or discrete operation in the context of a
/// block.
class Instr : public AcceptorExtend<Instr, Value> {
public:
  static const char NodeId;

  using AcceptorExtend::AcceptorExtend;

  virtual ~Instr() = default;

  types::Type *getType() const override { return nullptr; }
};

/// Instr representing setting a memory location.
class AssignInstr : public AcceptorExtend<AssignInstr, Instr> {
private:
  /// the left-hand side
  Var *lhs;
  /// the right-hand side
  ValuePtr rhs;

public:
  static const char NodeId;

  /// Constructs an assign instruction.
  /// @param lhs the left-hand side
  /// @param rhs the right-hand side
  /// @param field the field being set, may be empty
  /// @param name the instruction's name
  AssignInstr(Var *lhs, ValuePtr rhs, std::string name = "")
      : AcceptorExtend(std::move(name)), lhs(lhs), rhs(std::move(rhs)) {}

  /// @return the left-hand side
  const Var *getLhs() const { return lhs; }
  /// Sets the left-hand side
  /// @param l the new value
  void setLhs(Var *v) { lhs = v; }

  /// @return the right-hand side
  const ValuePtr &getRhs() const { return rhs; }
  /// Sets the right-hand side
  /// @param l the new value
  void setRhs(ValuePtr v) { rhs = std::move(v); }

  std::vector<Value *> getChildren() const override { return {rhs.get()}; }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  Value *doClone() const override;
};

/// Instr representing loading the field of a value.
class ExtractInstr : public AcceptorExtend<ExtractInstr, Instr> {
private:
  /// the value being manipulated
  ValuePtr val;
  /// the field
  std::string field;

public:
  static const char NodeId;

  /// Constructs a load instruction.
  /// @param val the value being manipulated
  /// @param field the field
  /// @param name the instruction's name
  explicit ExtractInstr(ValuePtr val, std::string field, std::string name = "")
      : AcceptorExtend(std::move(name)), val(std::move(val)), field(std::move(field)) {}

  types::Type *getType() const override;

  /// @return the location
  const ValuePtr &getVal() const { return val; }
  /// Sets the location.
  /// @param p the new value
  void setVal(ValuePtr p) { val = std::move(p); }

  /// @return the field
  const std::string &getField() { return field; }
  /// Sets the field.
  /// @param f the new field
  void setField(std::string f) { field = std::move(f); }

  std::vector<Value *> getChildren() const override { return {val.get()}; }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  Value *doClone() const override;
};

/// Instr representing setting the field of a value.
class InsertInstr : public AcceptorExtend<ExtractInstr, Instr> {
private:
  /// the value being manipulated
  ValuePtr lhs;
  /// the field
  std::string field;
  /// the value being inserted
  ValuePtr rhs;

public:
  static const char NodeId;

  /// Constructs a load instruction.
  /// @param lhs the value being manipulated
  /// @param field the field
  /// @param rhs the new value
  /// @param name the instruction's name
  explicit InsertInstr(ValuePtr lhs, std::string field, ValuePtr rhs,
                       std::string name = "")
      : AcceptorExtend(std::move(name)), lhs(std::move(lhs)), field(std::move(field)),
        rhs(std::move(rhs)) {}

  types::Type *getType() const override { return lhs->getType(); }

  /// @return the left-hand side
  const ValuePtr &getLhs() const { return lhs; }
  /// Sets the left-hand side.
  /// @param p the new value
  void setLhs(ValuePtr p) { lhs = std::move(p); }

  /// @return the right-hand side
  const ValuePtr &getRhs() const { return rhs; }
  /// Sets the right-hand side.
  /// @param p the new value
  void setRhs(ValuePtr p) { rhs = std::move(p); }

  /// @return the field
  const std::string &getField() { return field; }
  /// Sets the field.
  /// @param f the new field
  void setField(std::string f) { field = std::move(f); }

  std::vector<Value *> getChildren() const override { return {lhs.get(), rhs.get()}; }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  Value *doClone() const override;
};

/// Instr representing calling a function.
class CallInstr : public AcceptorExtend<CallInstr, Instr> {
public:
  using iterator = std::vector<ValuePtr>::iterator;
  using const_iterator = std::vector<ValuePtr>::const_iterator;
  using reference = std::vector<ValuePtr>::reference;
  using const_reference = std::vector<ValuePtr>::const_reference;

private:
  /// the function
  ValuePtr func;
  /// the arguments
  std::vector<ValuePtr> args;

public:
  static const char NodeId;

  /// Constructs a call instruction.
  /// @param func the function
  /// @param args the arguments
  /// @param name the instruction's name
  CallInstr(ValuePtr func, std::vector<ValuePtr> args, std::string name = "")
      : AcceptorExtend(std::move(name)), func(std::move(func)), args(std::move(args)) {}

  /// Constructs a call instruction with no arguments.
  /// @param func the function
  /// @param name the instruction's name
  explicit CallInstr(ValuePtr func, std::string name = "")
      : CallInstr(std::move(func), {}, std::move(name)) {}

  types::Type *getType() const override;

  /// @return the func
  const ValuePtr &getFunc() const { return func; }
  /// Sets the function.
  /// @param f the new function
  void setFunc(ValuePtr f) { func = std::move(f); }

  /// @return an iterator to the first argument
  iterator begin() { return args.begin(); }
  /// @return an iterator beyond the last argument
  iterator end() { return args.end(); }
  /// @return an iterator to the first argument
  const_iterator begin() const { return args.begin(); }
  /// @return an iterator beyond the last argument
  const_iterator end() const { return args.end(); }

  /// @return a reference to the first argument
  reference front() { return args.front(); }
  /// @return a reference to the last argument
  reference back() { return args.back(); }
  /// @return a reference to the first argument
  const_reference front() const { return args.front(); }
  /// @return a reference to the last argument
  const_reference back() const { return args.back(); }

  /// @return true if empty
  bool empty() const { return begin() == end(); }

  /// Inserts an argument at the given position.
  /// @param pos the position
  /// @param v the argument
  /// @return an iterator to the newly added argument
  iterator insert(iterator pos, ValuePtr v) { return args.insert(pos, std::move(v)); }
  /// Inserts an argument at the given position.
  /// @param pos the position
  /// @param v the argument
  /// @return an iterator to the newly added argument
  iterator insert(const_iterator pos, ValuePtr v) {
    return args.insert(pos, std::move(v));
  }
  /// Appends an argument.
  /// @param v the argument
  void push_back(ValuePtr v) { args.push_back(std::move(v)); }

  /// Sets the args.
  /// @param v the new args vector
  void setArgs(std::vector<ValuePtr> v) { args = std::move(v); }

  std::vector<Value *> getChildren() const override;

private:
  std::ostream &doFormat(std::ostream &os) const override;

  Value *doClone() const override;
};

/// Instr representing allocating an array on the stack.
class StackAllocInstr : public AcceptorExtend<StackAllocInstr, Instr> {
private:
  /// the array type
  types::Type *arrayType;
  /// number of elements to allocate
  ValuePtr count;

public:
  static const char NodeId;

  /// Constructs a stack allocation instruction.
  /// @param arrayType the type of the array
  /// @param count the number of elements
  StackAllocInstr(types::Type *arrayType, ValuePtr count, std::string name = "")
      : AcceptorExtend(std::move(name)), arrayType(arrayType), count(std::move(count)) {
  }

  types::Type *getType() const override { return arrayType; }

  /// @return the count
  const ValuePtr &getCount() const { return count; }
  /// Sets the count.
  /// @param c the new value
  void setCount(ValuePtr c) { count = std::move(c); }

  std::vector<Value *> getChildren() const override { return {count.get()}; }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  Value *doClone() const override;
};

/// Instr representing a Python yield expression.
class YieldInInstr : public AcceptorExtend<YieldInInstr, Instr> {
private:
  /// the type of the value being yielded in.
  types::Type *type;
  /// whether or not to suspend
  bool suspend;

public:
  static const char NodeId;

  /// Constructs a yield in instruction.
  /// @param type the type of the value being yielded in
  /// @param supsend whether to suspend
  /// @param name the instruction's name
  explicit YieldInInstr(types::Type *type, bool suspend = true, std::string name = "")
      : AcceptorExtend(std::move(name)), type(type), suspend(suspend) {}

  types::Type *getType() const override { return type; }

  /// @return true if the instruction suspends
  bool isSuspending() const { return suspend; }

  std::vector<Value *> getChildren() const override { return {}; }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  Value *doClone() const override;
};

/// Instr representing a ternary operator.
class TernaryInstr : public AcceptorExtend<TernaryInstr, Instr> {
private:
  /// the condition
  ValuePtr cond;
  /// the true value
  ValuePtr trueValue;
  /// the false value
  ValuePtr falseValue;

public:
  static const char NodeId;

  /// Constructs a ternary instruction.
  /// @param cond the condition
  /// @param trueValue the true value
  /// @param falseValue the false value
  /// @param name the instruction's name
  TernaryInstr(ValuePtr cond, ValuePtr trueValue, ValuePtr falseValue,
               std::string name = "")
      : AcceptorExtend(std::move(name)), cond(std::move(cond)),
        trueValue(std::move(trueValue)), falseValue(std::move(falseValue)) {}

  types::Type *getType() const override { return trueValue->getType(); }

  /// @return the condition
  const ValuePtr &getCond() const { return cond; }
  /// Sets the condition.
  /// @param v the new value
  void setCond(ValuePtr v) { cond = std::move(v); }

  /// @return the condition
  const ValuePtr &getTrueValue() const { return trueValue; }
  /// Sets the true value.
  /// @param v the new value
  void setTrueValue(ValuePtr v) { trueValue = std::move(v); }

  /// @return the false value
  const ValuePtr &getFalseValue() const { return falseValue; }
  /// Sets the value.
  /// @param v the new value
  void setFalseValue(ValuePtr v) { falseValue = std::move(v); }

  std::vector<Value *> getChildren() const override {
    return {cond.get(), trueValue.get(), falseValue.get()};
  }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  Value *doClone() const override;
};

class ControlFlowInstr : public AcceptorExtend<ControlFlowInstr, Instr> {
public:
  static const char NodeId;

  using AcceptorExtend::AcceptorExtend;

  /// @return all targets of the instruction. nullptr signifies that the instruction may
  /// return.
  virtual std::vector<Flow *> getTargets() const = 0;
};

/// Base for unconditional control-flow instructions.
class UnconditionalControlFlowInstr
    : public AcceptorExtend<UnconditionalControlFlowInstr, ControlFlowInstr> {
private:
  /// the target
  ValuePtr target;

public:
  static const char NodeId;

  /// Constructs a control flow instruction.
  /// @param target the flow being targeted
  explicit UnconditionalControlFlowInstr(ValuePtr target, std::string name = "")
      : AcceptorExtend(std::move(name)), target(std::move(target)) {}

  virtual ~UnconditionalControlFlowInstr() = default;

  /// @return all targets
  std::vector<Flow *> getTargets() const override;

  /// @return the target
  const ValuePtr &getTarget() const { return target; }
  /// Sets the count.
  /// @param f the new value
  void setTarget(ValuePtr f) { target = std::move(f); }

  std::vector<Value *> getChildren() const override {
    return target ? std::vector<Value *>{target.get()} : std::vector<Value *>();
  }
};

/// Instr representing a break statement.
class BreakInstr : public AcceptorExtend<BreakInstr, UnconditionalControlFlowInstr> {
public:
  static const char NodeId;

  using AcceptorExtend::AcceptorExtend;

private:
  std::ostream &doFormat(std::ostream &os) const override;

  Value *doClone() const override;
};

/// Instr representing a continue statement.
class ContinueInstr
    : public AcceptorExtend<ContinueInstr, UnconditionalControlFlowInstr> {
public:
  static const char NodeId;

  using AcceptorExtend::AcceptorExtend;

private:
  std::ostream &doFormat(std::ostream &os) const override;

  Value *doClone() const override;
};

/// Instr representing a return statement.
class ReturnInstr : public AcceptorExtend<ReturnInstr, UnconditionalControlFlowInstr> {
private:
  /// the value
  ValuePtr value;

public:
  static const char NodeId;

  explicit ReturnInstr(ValuePtr value = nullptr, std::string name = "")
      : AcceptorExtend(nullptr, std::move(name)), value(std::move(value)) {}

  /// @return the value
  const ValuePtr &getValue() const { return value; }
  /// Sets the value.
  /// @param v the new value
  void setValue(ValuePtr v) { value = std::move(v); }

  std::vector<Value *> getChildren() const override {
    return value ? std::vector<Value *>{value.get()} : std::vector<Value *>();
  }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  Value *doClone() const override;
};

/// Instr representing an unconditional jump.
class BranchInstr : public AcceptorExtend<BranchInstr, UnconditionalControlFlowInstr> {
public:
  static const char NodeId;

  using AcceptorExtend::AcceptorExtend;

private:
  std::ostream &doFormat(std::ostream &os) const override;

  Value *doClone() const override;
};

/// Instruction representing a conditional branch.
class CondBranchInstr : public AcceptorExtend<CondBranchInstr, ControlFlowInstr> {
public:
  struct Target {
    ValuePtr dst;
    ValuePtr cond;

    explicit Target(ValuePtr dst, ValuePtr cond = nullptr)
        : dst(std::move(dst)), cond(std::move(cond)) {}

    friend std::ostream &operator<<(std::ostream &os, const Target &t);
  };

  using iterator = std::vector<Target>::iterator;
  using reference = std::vector<Target>::reference;
  using const_iterator = std::vector<Target>::const_iterator;
  using const_reference = std::vector<Target>::const_reference;

private:
  /// the targets
  std::vector<Target> targets;

public:
  static const char NodeId;

  /// Constructs a conditional control flow instruction.
  /// @param targets the flows being targeted
  explicit CondBranchInstr(std::vector<Target> targets, std::string name = "")
      : AcceptorExtend(std::move(name)), targets(std::move(targets)) {}

  std::vector<Flow *> getTargets() const override;

  /// @return an iterator to the first target
  iterator begin() { return targets.begin(); }
  /// @return an iterator beyond the last target
  iterator end() { return targets.end(); }
  /// @return an iterator to the first target
  const_iterator begin() const { return targets.begin(); }
  /// @return an iterator beyond the last target
  const_iterator end() const { return targets.end(); }

  /// @return a reference to the first target
  reference front() { return targets.front(); }
  /// @return a reference to the last target
  reference back() { return targets.back(); }
  /// @return a reference to the first target
  const_reference front() const { return targets.front(); }
  /// @return a reference to the last target
  const_reference back() const { return targets.back(); }

  /// @return true if empty
  bool empty() const { return begin() == end(); }

  /// Inserts an target at the given position.
  /// @param pos the position
  /// @param v the target
  /// @return an iterator to the newly added target
  iterator insert(iterator pos, Target v) { return targets.insert(pos, std::move(v)); }
  /// Inserts an target at the given position.
  /// @param pos the position
  /// @param v the target
  /// @return an iterator to the newly added target
  iterator insert(const_iterator pos, Target v) {
    return targets.insert(pos, std::move(v));
  }
  /// Appends an target.
  /// @param v the target
  void push_back(Target v) { targets.push_back(std::move(v)); }

  /// Emplaces a target.
  /// @tparam Args the target constructor args
  template <typename... Args> void emplace_back(Args &&... args) {
    targets.emplace_back(std::forward<Args>(args)...);
  }

  std::vector<Value *> getChildren() const override;

private:
  std::ostream &doFormat(std::ostream &os) const override;

  Value *doClone() const override;
};

class YieldInstr : public AcceptorExtend<YieldInstr, Instr> {
private:
  /// the value
  ValuePtr value;

public:
  static const char NodeId;

  explicit YieldInstr(ValuePtr value = nullptr, std::string name = "")
      : AcceptorExtend(std::move(name)), value(std::move(value)) {}

  /// @return the value
  const ValuePtr &getValue() const { return value; }
  /// Sets the value.
  /// @param v the new value
  void setValue(ValuePtr v) { value = std::move(v); }

  std::vector<Value *> getChildren() const override { return {value.get()}; }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  Value *doClone() const override;
};

class ThrowInstr : public AcceptorExtend<ThrowInstr, Instr> {
private:
  /// the value
  ValuePtr value;

public:
  static const char NodeId;

  explicit ThrowInstr(ValuePtr value = nullptr, std::string name = "")
      : AcceptorExtend(std::move(name)), value(std::move(value)) {}

  /// @return the value
  const ValuePtr &getValue() const { return value; }
  /// Sets the value.
  /// @param v the new value
  void setValue(ValuePtr v) { value = std::move(v); }

  std::vector<Value *> getChildren() const override { return {value.get()}; }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  Value *doClone() const override;
};

/// Instr that contains a flow and value.
class FlowInstr : public AcceptorExtend<FlowInstr, Instr> {
private:
  /// the flow
  FlowPtr flow;
  /// the output value
  ValuePtr val;

public:
  static const char NodeId;

  /// Constructs a flow value.
  /// @param flow the flow
  /// @param val the output value
  /// @param name the name
  explicit FlowInstr(FlowPtr flow, ValuePtr val, std::string name = "")
      : AcceptorExtend(std::move(name)), flow(std::move(flow)), val(std::move(val)) {}

  types::Type *getType() const override { return val->getType(); }

  /// @return the flow
  const FlowPtr &getFlow() const { return flow; }
  /// Sets the flow.
  /// @param f the new flow
  void setFlow(FlowPtr f) { flow = std::move(f); }

  /// @return the value
  const ValuePtr &getValue() const { return val; }
  /// Sets the value.
  /// @param v the new value
  void setValue(ValuePtr v) { val = std::move(v); }

  std::vector<Value *> getChildren() const override { return {flow.get(), val.get()}; }

private:
  std::ostream &doFormat(std::ostream &os) const override;

  Value *doClone() const override;
};

/// Instruction representing a phi node.
class PhiInstr : public AcceptorExtend<PhiInstr, Instr> {
public:
  struct Predecessor {
    ValuePtr pred;
    ValuePtr val;

    Predecessor(ValuePtr pred, ValuePtr val)
        : pred(std::move(pred)), val(std::move(val)) {}

    friend std::ostream &operator<<(std::ostream &os, const Predecessor &p);
  };

  using iterator = std::vector<Predecessor>::iterator;
  using reference = std::vector<Predecessor>::reference;
  using const_iterator = std::vector<Predecessor>::const_iterator;
  using const_reference = std::vector<Predecessor>::const_reference;

private:
  /// the targets
  std::vector<Predecessor> predecessors;

public:
  static const char NodeId;

  /// Constructs a conditional control flow instruction.
  /// @param predecessors the flows being predecessored
  explicit PhiInstr(std::vector<Predecessor> predecessors, std::string name = "")
      : AcceptorExtend(std::move(name)), predecessors(std::move(predecessors)) {}

  /// @return an iterator to the first predecessor
  iterator begin() { return predecessors.begin(); }
  /// @return an iterator beyond the last predecessor
  iterator end() { return predecessors.end(); }
  /// @return an iterator to the first predecessor
  const_iterator begin() const { return predecessors.begin(); }
  /// @return an iterator beyond the last predecessor
  const_iterator end() const { return predecessors.end(); }

  /// @return a reference to the first predecessor
  reference front() { return predecessors.front(); }
  /// @return a reference to the last predecessor
  reference back() { return predecessors.back(); }
  /// @return a reference to the first predecessor
  const_reference front() const { return predecessors.front(); }
  /// @return a reference to the last predecessor
  const_reference back() const { return predecessors.back(); }

  /// @return true if empty
  bool empty() const { return begin() == end(); }

  /// Inserts an predecessor at the given position.
  /// @param pos the position
  /// @param v the predecessor
  /// @return an iterator to the newly added predecessor
  iterator insert(iterator pos, Predecessor v) {
    return predecessors.insert(pos, std::move(v));
  }
  /// Inserts an predecessor at the given position.
  /// @param pos the position
  /// @param v the predecessor
  /// @return an iterator to the newly added predecessor
  iterator insert(const_iterator pos, Predecessor v) {
    return predecessors.insert(pos, std::move(v));
  }
  /// Appends an predecessor.
  /// @param v the predecessor
  void push_back(Predecessor v) { predecessors.push_back(std::move(v)); }

  /// Emplaces a predecessor.
  /// @tparam Args the predecessor constructor args
  template <typename... Args> void emplace_back(Args &&... args) {
    predecessors.emplace_back(std::forward<Args>(args)...);
  }

  std::vector<Value *> getChildren() const override;

private:
  std::ostream &doFormat(std::ostream &os) const override;

  Value *doClone() const override;
};

} // namespace ir
} // namespace seq

// See https://github.com/fmtlib/fmt/issues/1283.
namespace fmt {
template <typename Char>
struct formatter<seq::ir::CondBranchInstr::Target, Char>
    : fmt::v6::internal::fallback_formatter<seq::ir::CondBranchInstr::Target, Char> {};

template <typename Char>
struct formatter<seq::ir::PhiInstr::Predecessor, Char>
    : fmt::v6::internal::fallback_formatter<seq::ir::PhiInstr::Predecessor, Char> {};
} // namespace fmt
