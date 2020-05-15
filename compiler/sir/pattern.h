#pragma once

#include <memory>
#include <string>
#include <vector>

#include "base.h"
#include "restypes/types.h"
#include "util/common.h"

namespace seq {
namespace ir {

class Var;
class Expression;

class Pattern : AttributeHolder {
private:
  std::shared_ptr<restypes::Type> type;

public:
  explicit Pattern(std::shared_ptr<restypes::Type> type);

  virtual std::string textRepresentation() const;
};

class WildcardPattern : Pattern {
  std::weak_ptr<Var> var;

public:
  explicit WildcardPattern(std::weak_ptr<Var> var);

  std::string textRepresentation() const;
};

class BoundPattern : Pattern {
private:
  std::weak_ptr<Var> var;
  std::shared_ptr<Pattern> pattern;

public:
  explicit BoundPattern(std::weak_ptr<Var> var,
                        std::shared_ptr<Pattern> pattern);

  std::string textRepresentation() const;
};

class StarPattern : Pattern {
public:
  std::string textRepresentation() const;
};

class IntPattern : Pattern {
private:
  seq_int_t value;

public:
  explicit IntPattern(seq_int_t value);

  std::string textRepresentation() const;
};

class BoolPattern : Pattern {
private:
  bool value;

public:
  explicit BoolPattern(bool value);

  std::string textRepresentation() const;
};

class StrPattern : Pattern {
private:
  std::string value;

public:
  explicit StrPattern(std::string value);

  std::string textRepresentation() const;
};

class RecordPattern : Pattern {
private:
  std::vector<std::shared_ptr<Pattern>> patterns;

public:
  explicit RecordPattern(std::vector<std::shared_ptr<Pattern>> patterns);

  std::string textRepresentation() const;
};

class ArrayPattern : Pattern {
private:
  std::vector<std::shared_ptr<Pattern>> patterns;

public:
  explicit ArrayPattern(std::vector<std::shared_ptr<Pattern>> patterns);

  std::string textRepresentation() const;
};

class OptionalPattern : Pattern {
private:
  std::shared_ptr<Pattern> pattern;

public:
  explicit OptionalPattern(std::shared_ptr<Pattern> pattern);

  std::string textRepresentation() const;
};

class RangePattern : Pattern {
private:
  seq_int_t a;
  seq_int_t b;

public:
  explicit RangePattern(seq_int_t a, seq_int_t b);

  std::string textRepresentation() const;
};

class OrPattern : Pattern {
private:
  std::vector<std::shared_ptr<Pattern>> patterns;

public:
  explicit OrPattern(std::vector<std::shared_ptr<Pattern>> patterns);

  std::string textRepresentation() const;
};

class GuardedPattern : Pattern {
private:
  std::shared_ptr<Pattern> pattern;
  std::shared_ptr<Expression> expr;

public:
  explicit GuardedPattern(std::shared_ptr<Pattern> pattern,
                          std::shared_ptr<Expression> expr);

  std::string textRepresentation() const;
};
} // namespace ir
} // namespace seq