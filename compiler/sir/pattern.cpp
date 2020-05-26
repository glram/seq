#include <algorithm>
#include <iterator>
#include <sstream>

#include "expr.h"
#include "pattern.h"
#include "var.h"

using namespace seq;
using namespace ir;

Pattern::Pattern(std::shared_ptr<restypes::Type> type) : type{type} {}

std::shared_ptr<restypes::Type> Pattern::getType() { return type; }

std::string Pattern::textRepresentation() const {
  return AttributeHolder::textRepresentation();
}

WildcardPattern::WildcardPattern(std::shared_ptr<restypes::Type> type)
    : Pattern{type}, var{std::make_shared<Var>(type)} {}

WildcardPattern::WildcardPattern()
    : Pattern{restypes::kAnyType}, var{std::make_shared<Var>(
                                       restypes::kAnyType)} {}

std::string WildcardPattern::textRepresentation() const {
  std::stringstream stream;
  stream << Pattern::textRepresentation() << " _";
  return stream.str();
}
BoundPattern::BoundPattern(std::shared_ptr<Pattern> pattern)
    : Pattern{pattern->getType()}, pattern{pattern}, var{std::make_shared<Var>(
                                                         pattern->getType())} {}

std::string BoundPattern::textRepresentation() const {
  std::stringstream stream;
  stream << Pattern::textRepresentation() << " ("
         << pattern->textRepresentation() << ")";
  return stream.str();
}

StarPattern::StarPattern() : Pattern{restypes::kAnyType} {}

std::string StarPattern::textRepresentation() const {
  std::stringstream stream;
  stream << Pattern::textRepresentation() << " ...";
  return stream.str();
}
IntPattern::IntPattern(seq_int_t value)
    : Pattern{std::static_pointer_cast<restypes::Type>(restypes::kIntType)},
      value{value} {}

std::string IntPattern::textRepresentation() const {
  std::stringstream stream;
  stream << Pattern::textRepresentation() << " " << value;
  return stream.str();
}
BoolPattern::BoolPattern(bool value)
    : Pattern{std::static_pointer_cast<restypes::Type>(restypes::kBoolType)},
      value{value} {}

std::string BoolPattern::textRepresentation() const {
  std::stringstream stream;
  stream << Pattern::textRepresentation() << " "
         << ((value) ? "true" : "false");
  return stream.str();
}

StrPattern::StrPattern(std::string value)
    : Pattern{std::static_pointer_cast<restypes::Type>(restypes::kStringType)},
      value{value} {}

std::string StrPattern::textRepresentation() const {
  std::stringstream stream;
  stream << Pattern::textRepresentation() << " " << value;
  return stream.str();
}

// TODO - what type is this
RecordPattern::RecordPattern(std::vector<std::shared_ptr<Pattern>> patterns)
    : Pattern{restypes::kAnyType}, patterns{patterns} {}

RecordPattern::RecordPattern(RecordPattern &other)
    : Pattern{other.getType()}, patterns{} {
  std::copy(other.patterns.begin(), other.patterns.end(),
            std::back_inserter(patterns));
}

std::string RecordPattern::textRepresentation() const {
  std::stringstream stream;
  stream << Pattern::textRepresentation() << " {";
  for (auto it = patterns.begin(); it != patterns.end(); it++) {
    stream << (*it)->textRepresentation();
    if (it + 1 != patterns.end())
      stream << ", ";
  }
  stream << "}";
  return stream.str();
}
ArrayPattern::ArrayPattern(std::vector<std::shared_ptr<Pattern>> patterns)
    : Pattern{restypes::kAnyType}, patterns{patterns} {}

ArrayPattern::ArrayPattern(ArrayPattern &other)
    : Pattern{other.getType()}, patterns{} {
  std::copy(other.patterns.begin(), other.patterns.end(),
            std::back_inserter(patterns));
}

std::string ArrayPattern::textRepresentation() const {
  std::stringstream stream;
  stream << Pattern::textRepresentation() << " [";
  for (auto it = patterns.begin(); it != patterns.end(); it++) {
    stream << (*it)->textRepresentation();
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
  stream << Pattern::textRepresentation() << "("
         << pattern->textRepresentation() << ")?";
  return stream.str()
}
RangePattern::RangePattern(seq_int_t a, seq_int_t b)
    : Pattern(restypes::kAnyType), a{a}, b{b} {}

std::string RangePattern::textRepresentation() const {
  std::stringstream stream;
  stream << Pattern::textRepresentation() << " " << a << "..." << b;
  return stream.str();
}
OrPattern::OrPattern(std::vector<std::shared_ptr<Pattern>> patterns)
    : Pattern{restypes::kAnyType}, patterns{patterns} {}

OrPattern::OrPattern(OrPattern &other) : Pattern{other.getType()}, patterns{} {
  std::copy(other.patterns.begin(), other.patterns.end(),
            std::back_inserter(patterns));
}

std::string OrPattern::textRepresentation() const {
  std::stringstream stream;
  stream << Pattern::textRepresentation() << " ";
  for (auto it = patterns.begin(); it != patterns.end(); it++) {
    stream << (*it)->textRepresentation();
    if (it + 1 != patterns.end())
      stream << " or ";
  }
  return stream.str();
}
GuardedPattern::GuardedPattern(std::shared_ptr<Pattern> pattern,
                               std::shared_ptr<Expression> expr)
    : Pattern{pattern->getType()}, pattern{pattern}, expr{expr} {}

std::string GuardedPattern::textRepresentation() const {
  std::stringstream stream;
  stream << Pattern::textRepresentation() << " ("
         << pattern->textRepresentation() << ") if "
         << expr->textRepresentation();
  return stream.str();
}
