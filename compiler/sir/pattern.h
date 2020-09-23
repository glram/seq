#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base.h"
#include "types/types.h"
#include "util/common.h"

#include "codegen/codegen.h"

namespace seq {
namespace ir {

class Var;
class Operand;

class Pattern : public AttributeHolder<Pattern> {
public:
  virtual ~Pattern() = default;

  virtual void accept(codegen::CodegenVisitor &v) { v.visit(getShared()); }

  std::string referenceString() const override { return "pattern"; }
};

class WildcardPattern : public Pattern {
  std::shared_ptr<Var> var;

public:
  explicit WildcardPattern(std::shared_ptr<types::Type> type);
  WildcardPattern();

  void accept(codegen::CodegenVisitor &v) override {
    v.visit(std::static_pointer_cast<WildcardPattern>(getShared()));
  }

  std::shared_ptr<Var> getVar() { return var; };

  std::string textRepresentation() const override;
};

class BoundPattern : public Pattern {
private:
  std::shared_ptr<Var> var;
  std::shared_ptr<Pattern> pattern;

public:
  explicit BoundPattern(std::shared_ptr<Pattern> p);

  void accept(codegen::CodegenVisitor &v) override {
    v.visit(std::static_pointer_cast<BoundPattern>(getShared()));
  }

  std::shared_ptr<Var> getVar() { return var; };

  std::string textRepresentation() const override;
};

class StarPattern : public Pattern {
public:
  StarPattern() = default;

  void accept(codegen::CodegenVisitor &v) override {
    v.visit(std::static_pointer_cast<StarPattern>(getShared()));
  }

  std::string textRepresentation() const override;
};

class IntPattern : public Pattern {
private:
  seq_int_t value;

public:
  explicit IntPattern(seq_int_t value) : value(value) {}

  void accept(codegen::CodegenVisitor &v) override {
    v.visit(std::static_pointer_cast<IntPattern>(getShared()));
  }

  std::string textRepresentation() const override;
};

class BoolPattern : public Pattern {
private:
  bool value;

public:
  explicit BoolPattern(bool value) : value(value) {}

  void accept(codegen::CodegenVisitor &v) override {
    v.visit(std::static_pointer_cast<BoolPattern>(getShared()));
  }

  std::string textRepresentation() const override;
};

class StrPattern : public Pattern {
private:
  std::string value;

public:
  explicit StrPattern(std::string value) : value(std::move(value)) {}

  void accept(codegen::CodegenVisitor &v) override {
    v.visit(std::static_pointer_cast<StrPattern>(getShared()));
  }

  std::string textRepresentation() const override;
};

class SeqPattern : public Pattern {
private:
  std::string value;

public:
  explicit SeqPattern(std::string value) : value(std::move(value)) {}

  void accept(codegen::CodegenVisitor &v) override {
    v.visit(std::static_pointer_cast<SeqPattern>(getShared()));
  }

  std::string textRepresentation() const override;
};

class RecordPattern : public Pattern {
private:
  std::vector<std::shared_ptr<Pattern>> patterns;

public:
  explicit RecordPattern(std::vector<std::shared_ptr<Pattern>> patterns)
      : patterns(std::move(patterns)) {}

  void accept(codegen::CodegenVisitor &v) override {
    v.visit(std::static_pointer_cast<RecordPattern>(getShared()));
  }

  std::string textRepresentation() const override;
};

class ArrayPattern : public Pattern {
private:
  std::vector<std::shared_ptr<Pattern>> patterns;

public:
  explicit ArrayPattern(std::vector<std::shared_ptr<Pattern>> patterns)
      : patterns(std::move(patterns)) {}

  void accept(codegen::CodegenVisitor &v) override {
    v.visit(std::static_pointer_cast<ArrayPattern>(getShared()));
  }

  std::string textRepresentation() const override;
};

class OptionalPattern : public Pattern {
private:
  std::shared_ptr<Pattern> pattern;

public:
  explicit OptionalPattern(std::shared_ptr<Pattern> pattern)
      : pattern(std::move(pattern)) {}

  void accept(codegen::CodegenVisitor &v) override {
    v.visit(std::static_pointer_cast<OptionalPattern>(getShared()));
  }

  std::string textRepresentation() const override;
};

class RangePattern : public Pattern {
private:
  seq_int_t a;
  seq_int_t b;

public:
  explicit RangePattern(seq_int_t a, seq_int_t b) : a(a), b(b) {}

  void accept(codegen::CodegenVisitor &v) override {
    v.visit(std::static_pointer_cast<RangePattern>(getShared()));
  }

  std::string textRepresentation() const override;
};

class OrPattern : public Pattern {
private:
  std::vector<std::shared_ptr<Pattern>> patterns;

public:
  explicit OrPattern(std::vector<std::shared_ptr<Pattern>> patterns)
      : patterns(std::move(patterns)) {}

  void accept(codegen::CodegenVisitor &v) override {
    v.visit(std::static_pointer_cast<OrPattern>(getShared()));
  }

  std::string textRepresentation() const override;
};

class GuardedPattern : public Pattern {
private:
  std::shared_ptr<Pattern> pattern;
  std::shared_ptr<Operand> operand;

public:
  explicit GuardedPattern(std::shared_ptr<Pattern> pattern,
                          std::shared_ptr<Operand> operand)
      : pattern(std::move(pattern)), operand(std::move(operand)) {}

  void accept(codegen::CodegenVisitor &v) override {
    v.visit(std::static_pointer_cast<GuardedPattern>(getShared()));
  }

  std::string textRepresentation() const override;
};

} // namespace ir
} // namespace seq
