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

namespace common {
class IRVisitor;
}

class Var;
class Operand;

class Pattern : public AttributeHolder<Pattern> {
private:
  int id;
  static int currentId;

public:
  Pattern() : id(currentId++){};
  virtual ~Pattern() = default;

  static void resetId();

  virtual void accept(common::IRVisitor &v);

  int getId() const { return id; }

  std::string referenceString() const override;
};

class WildcardPattern : public Pattern {
  std::shared_ptr<Var> var;

public:
  explicit WildcardPattern(std::shared_ptr<types::Type> type);

  void accept(common::IRVisitor &v) override;

  std::shared_ptr<Var> getVar() { return var; };

  std::string textRepresentation() const override;
};

class BoundPattern : public Pattern {
private:
  std::shared_ptr<Var> var;
  std::shared_ptr<Pattern> pattern;

public:
  BoundPattern(std::shared_ptr<Pattern> p, std::shared_ptr<types::Type> type);

  void accept(common::IRVisitor &v) override;

  std::shared_ptr<Var> getVar() { return var; };

  std::string textRepresentation() const override;
};

class StarPattern : public Pattern {
public:
  StarPattern() = default;

  void accept(common::IRVisitor &v) override;

  std::string textRepresentation() const override;
};

class IntPattern : public Pattern {
private:
  seq_int_t value;

public:
  explicit IntPattern(seq_int_t value) : value(value) {}

  void accept(common::IRVisitor &v) override;

  seq_int_t getValue() const { return value; }

  std::string textRepresentation() const override;
};

class BoolPattern : public Pattern {
private:
  bool value;

public:
  explicit BoolPattern(bool value) : value(value) {}

  void accept(common::IRVisitor &v) override;

  bool getValue() const { return value; }

  std::string textRepresentation() const override;
};

class StrPattern : public Pattern {
private:
  std::string value;

public:
  explicit StrPattern(std::string value) : value(std::move(value)) {}

  void accept(common::IRVisitor &v) override;

  std::string getValue() const { return value; }

  std::string textRepresentation() const override;
};

class SeqPattern : public Pattern {
private:
  std::string value;

public:
  explicit SeqPattern(std::string value) : value(std::move(value)) {}

  void accept(common::IRVisitor &v) override;

  std::string getValue() const { return value; }

  std::string textRepresentation() const override;
};

class RecordPattern : public Pattern {
private:
  std::vector<std::shared_ptr<Pattern>> patterns;
  std::shared_ptr<types::Type> type;

public:
  explicit RecordPattern(std::vector<std::shared_ptr<Pattern>> patterns,
                         std::shared_ptr<types::Type> type)
      : patterns(std::move(patterns)), type(std::move(type)) {}

  void accept(common::IRVisitor &v) override;

  std::vector<std::shared_ptr<Pattern>> getPatterns() { return patterns; }
  std::shared_ptr<types::Type> getType() { return type; }

  std::string textRepresentation() const override;
};

class ArrayPattern : public Pattern {
private:
  std::vector<std::shared_ptr<Pattern>> patterns;
  std::shared_ptr<types::Type> type;

public:
  explicit ArrayPattern(std::vector<std::shared_ptr<Pattern>> patterns,
                        std::shared_ptr<types::Type> type)
      : patterns(std::move(patterns)), type(std::move(type)) {}

  void accept(common::IRVisitor &v) override;

  std::vector<std::shared_ptr<Pattern>> getPatterns() { return patterns; }
  std::shared_ptr<types::Type> getType() { return type; }

  std::string textRepresentation() const override;
};

class OptionalPattern : public Pattern {
private:
  std::shared_ptr<Pattern> pattern;
  std::shared_ptr<types::Type> type;

public:
  explicit OptionalPattern(std::shared_ptr<Pattern> pattern,
                           std::shared_ptr<types::Type> type)
      : pattern(std::move(pattern)), type(std::move(type)) {}

  void accept(common::IRVisitor &v) override;
  std::shared_ptr<types::Type> getType() { return type; }

  std::string textRepresentation() const override;
};

class RangePattern : public Pattern {
private:
  seq_int_t lower;
  seq_int_t higher;

public:
  explicit RangePattern(seq_int_t a, seq_int_t b) : lower(a), higher(b) {}

  void accept(common::IRVisitor &v) override;

  seq_int_t getLower() const { return lower; }
  seq_int_t getHigher() const { return higher; }

  std::string textRepresentation() const override;
};

class OrPattern : public Pattern {
private:
  std::vector<std::shared_ptr<Pattern>> patterns;

public:
  explicit OrPattern(std::vector<std::shared_ptr<Pattern>> patterns)
      : patterns(std::move(patterns)) {}

  void accept(common::IRVisitor &v) override;

  std::vector<std::shared_ptr<Pattern>> getPatterns() { return patterns; }

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

  void accept(common::IRVisitor &v) override;

  std::shared_ptr<Pattern> getPattern() { return pattern; }
  std::shared_ptr<Operand> getOperand() { return operand; }

  std::string textRepresentation() const override;
};

} // namespace ir
} // namespace seq
