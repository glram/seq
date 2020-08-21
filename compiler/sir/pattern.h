#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base.h"
#include "types/types.h"
#include "util/common.h"

namespace seq {
namespace ir {

class Var;
class Operand;

class Pattern : public AttributeHolder<Pattern> {
private:
  std::shared_ptr<types::Type> type;

public:
  explicit Pattern(std::shared_ptr<types::Type> t) : type(std::move(t)) {}

  std::shared_ptr<types::Type> getType() { return type; }
  virtual std::string textRepresentation() const override;
};

class WildcardPattern : Pattern {
  std::shared_ptr<Var> var;

public:
  explicit WildcardPattern(std::shared_ptr<types::Type> type);
  WildcardPattern();

  std::string textRepresentation() const override;
};

class BoundPattern : Pattern {
private:
  std::shared_ptr<Var> var;
  std::shared_ptr<Pattern> pattern;

public:
  explicit BoundPattern(std::shared_ptr<Pattern> p);

  std::string textRepresentation() const override;
};

class StarPattern : Pattern {
public:
  StarPattern() : Pattern(types::kAnyType) {}

  std::string textRepresentation() const override;
};

class IntPattern : Pattern {
private:
  seq_int_t value;

public:
  explicit IntPattern(seq_int_t value)
      : Pattern(types::kIntType), value(value) {}

  std::string textRepresentation() const override;
};

class BoolPattern : Pattern {
private:
  bool value;

public:
  explicit BoolPattern(bool value) : Pattern(types::kBoolType), value(value) {}

  std::string textRepresentation() const override;
};

class StrPattern : Pattern {
private:
  std::string value;

public:
  explicit StrPattern(std::string value)
      : Pattern(types::kStringType), value(std::move(value)) {}

  std::string textRepresentation() const override;
};

class RecordPattern : Pattern {
private:
  std::vector<std::shared_ptr<Pattern>> patterns;

public:
  explicit RecordPattern(std::vector<std::shared_ptr<Pattern>> patterns)
      : Pattern(types::kAnyType), patterns(std::move(patterns)) {}

  std::string textRepresentation() const override;
};

class ArrayPattern : Pattern {
private:
  std::vector<std::shared_ptr<Pattern>> patterns;

public:
  explicit ArrayPattern(std::vector<std::shared_ptr<Pattern>> patterns)
      : Pattern(types::kAnyType), patterns(std::move(patterns)) {}

  std::string textRepresentation() const override;
};

class OptionalPattern : Pattern {
private:
  std::shared_ptr<Pattern> pattern;

public:
  explicit OptionalPattern(std::shared_ptr<Pattern> pattern)
      : Pattern(pattern->getType()), pattern(std::move(pattern)) {}

  std::string textRepresentation() const override;
};

class RangePattern : Pattern {
private:
  seq_int_t a;
  seq_int_t b;

public:
  explicit RangePattern(seq_int_t a, seq_int_t b)
      : Pattern(types::kAnyType), a(a), b(b) {}

  std::string textRepresentation() const override;
};

class OrPattern : Pattern {
private:
  std::vector<std::shared_ptr<Pattern>> patterns;

public:
  explicit OrPattern(std::vector<std::shared_ptr<Pattern>> patterns)
      : Pattern(types::kAnyType), patterns(std::move(patterns)) {}

  std::string textRepresentation() const override;
};

class GuardedPattern : Pattern {
private:
  std::shared_ptr<Pattern> pattern;
  std::shared_ptr<Operand> operand;

public:
  explicit GuardedPattern(std::shared_ptr<Pattern> pattern,
                          std::shared_ptr<Operand> operand)
      : Pattern(pattern->getType()), pattern(std::move(pattern)),
        operand(std::move(operand)) {}

  std::string textRepresentation() const override;
};

} // namespace ir
} // namespace seq
