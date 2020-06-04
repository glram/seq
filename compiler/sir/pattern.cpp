#include <algorithm>
#include <iterator>
#include <sstream>

#include "operand.h"
#include "pattern.h"
#include "var.h"

using namespace seq;
using namespace ir;

// TODO: nested attributes?

Pattern::Pattern(std::shared_ptr<types::Type> type) : type{type} {}

std::shared_ptr<types::Type> Pattern::getType() { return type; }

std::string Pattern::textRepresentation() const { return ""; }

WildcardPattern::WildcardPattern(std::shared_ptr<types::Type> type)
    : Pattern{type}, var{std::make_shared<Var>(type)} {}

WildcardPattern::WildcardPattern()
    : Pattern{types::kAnyType}, var{std::make_shared<Var>(types::kAnyType)} {}

std::string WildcardPattern::textRepresentation() const { return "_"; }
BoundPattern::BoundPattern(std::shared_ptr<Pattern> pattern)
    : Pattern{pattern->getType()}, pattern{pattern}, var{std::make_shared<Var>(
                                                         pattern->getType())} {}

std::string BoundPattern::textRepresentation() const {
  std::stringstream stream;
  stream << "((" << pattern->textRepresentation() << ")->"
         << var->referenceString() << ")";
  return stream.str();
}

StarPattern::StarPattern() : Pattern{types::kAnyType} {}

std::string StarPattern::textRepresentation() const { return "..."; }
IntPattern::IntPattern(seq_int_t value)
    : Pattern{std::static_pointer_cast<types::Type>(types::kIntType)},
      value{value} {}

std::string IntPattern::textRepresentation() const {
  std::stringstream stream;
  stream << value;
  return stream.str();
}
BoolPattern::BoolPattern(bool value)
    : Pattern{std::static_pointer_cast<types::Type>(types::kBoolType)},
      value{value} {}

std::string BoolPattern::textRepresentation() const {
  return ((value) ? "true" : "false");
}

StrPattern::StrPattern(std::string value)
    : Pattern{std::static_pointer_cast<types::Type>(types::kStringType)},
      value{value} {}

std::string StrPattern::textRepresentation() const {
  std::stringstream stream;
  stream << "'" << value << "'";
  return stream.str();
}

// TODO - what type is this
RecordPattern::RecordPattern(std::vector<std::shared_ptr<Pattern>> patterns)
    : Pattern{types::kAnyType}, patterns{patterns} {}

RecordPattern::RecordPattern(RecordPattern &other)
    : Pattern{other.getType()}, patterns{} {
  std::copy(other.patterns.begin(), other.patterns.end(),
            std::back_inserter(patterns));
}

std::string RecordPattern::textRepresentation() const {
  std::stringstream stream;
  stream << "{";
  for (auto it = patterns.begin(); it != patterns.end(); it++) {
    stream << "(" << (*it)->textRepresentation() << ")";
    if (it + 1 != patterns.end())
      stream << ", ";
  }
  stream << "}";
  return stream.str();
}
ArrayPattern::ArrayPattern(std::vector<std::shared_ptr<Pattern>> patterns)
    : Pattern{types::kAnyType}, patterns{patterns} {}

ArrayPattern::ArrayPattern(ArrayPattern &other)
    : Pattern{other.getType()}, patterns{} {
  std::copy(other.patterns.begin(), other.patterns.end(),
            std::back_inserter(patterns));
}

std::string ArrayPattern::textRepresentation() const {
  std::stringstream stream;
  stream << "[";
  for (auto it = patterns.begin(); it != patterns.end(); it++) {
    stream << "(" << (*it)->textRepresentation() << ")";
    if (it + 1 != patterns.end())
      stream << ", ";
  }
  stream << "]";
  return stream.str();
}

OptionalPattern::OptionalPattern(std::shared_ptr<Pattern> pattern)
    : Pattern{pattern->getType()}, pattern{pattern} {}

std::string OptionalPattern::textRepresentation() const {
  std::stringstream stream;
  stream << pattern->textRepresentation() << "?";
  return stream.str();
}

RangePattern::RangePattern(seq_int_t a, seq_int_t b)
    : Pattern(types::kAnyType), a{a}, b{b} {}

std::string RangePattern::textRepresentation() const {
  std::stringstream stream;
  stream << a << "..." << b;
  return stream.str();
}
OrPattern::OrPattern(std::vector<std::shared_ptr<Pattern>> patterns)
    : Pattern{types::kAnyType}, patterns{patterns} {}

OrPattern::OrPattern(OrPattern &other) : Pattern{other.getType()}, patterns{} {
  std::copy(other.patterns.begin(), other.patterns.end(),
            std::back_inserter(patterns));
}

std::string OrPattern::textRepresentation() const {
  std::stringstream stream;
  for (auto it = patterns.begin(); it != patterns.end(); it++) {
    stream << "(" << (*it)->textRepresentation() << ")";
    if (it + 1 != patterns.end())
      stream << " or ";
  }
  return stream.str();
}
GuardedPattern::GuardedPattern(std::shared_ptr<Pattern> pattern,
                               std::shared_ptr<Operand> operand)
    : Pattern{pattern->getType()}, pattern{pattern}, operand{operand} {}

std::string GuardedPattern::textRepresentation() const {
  std::stringstream stream;
  stream << "(" << pattern->textRepresentation() << ") if "
         << operand->textRepresentation();
  return stream.str();
}
