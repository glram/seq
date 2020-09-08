#include "util/fmt/format.h"

#include "operand.h"
#include "pattern.h"
#include "var.h"

namespace seq {
namespace ir {

WildcardPattern::WildcardPattern(std::shared_ptr<types::Type> type)
    : var(std::make_shared<Var>(type)) {}

WildcardPattern::WildcardPattern() : var(std::make_shared<Var>(types::kAnyType)) {}

std::string WildcardPattern::textRepresentation() const { return "_"; }

BoundPattern::BoundPattern(std::shared_ptr<Pattern> p)
    : var(std::make_shared<Var>(types::kAnyType)), pattern(p) {}

std::string BoundPattern::textRepresentation() const {
  return fmt::format(FMT_STRING("(({}->{})"), pattern->textRepresentation(),
                     var->referenceString());
}

std::string StarPattern::textRepresentation() const { return "..."; }

std::string IntPattern::textRepresentation() const {
  return fmt::format(FMT_STRING("{}"), value);
}

std::string BoolPattern::textRepresentation() const {
  return ((value) ? "true" : "false");
}

std::string StrPattern::textRepresentation() const {
  return fmt::format(FMT_STRING("'{}'"), value);
}

std::string SeqPattern::textRepresentation() const {
  return fmt::format(FMT_STRING("s'{}'"), value);
}

std::string RecordPattern::textRepresentation() const {
  fmt::memory_buffer buf;
  buf.push_back('{');
  for (auto it = patterns.begin(); it != patterns.end(); it++) {
    fmt::format_to(buf, FMT_STRING("({})"), (*it)->textRepresentation());
    if (it + 1 != patterns.end())
      fmt::format_to(buf, FMT_STRING(", "));
  }
  buf.push_back('}');
  return std::string(buf.data(), buf.size());
}

std::string ArrayPattern::textRepresentation() const {
  fmt::memory_buffer buf;
  buf.push_back('[');
  for (auto it = patterns.begin(); it != patterns.end(); it++) {
    fmt::format_to(buf, FMT_STRING("({})"), (*it)->textRepresentation());
    if (it + 1 != patterns.end())
      fmt::format_to(buf, FMT_STRING(", "));
  }
  buf.push_back(']');
  return std::string(buf.data(), buf.size());
}

std::string OptionalPattern::textRepresentation() const {
  return fmt::format(FMT_STRING("{}?"), pattern->textRepresentation());
}

std::string RangePattern::textRepresentation() const {
  return fmt::format(FMT_STRING("{}...{}"), a, b);
}

std::string OrPattern::textRepresentation() const {
  fmt::memory_buffer buf;
  for (auto it = patterns.begin(); it != patterns.end(); it++) {
    fmt::format_to(buf, FMT_STRING("({})"), (*it)->textRepresentation());
    if (it + 1 != patterns.end())
      fmt::format_to(buf, FMT_STRING(" or "));
  }
  return std::string(buf.data(), buf.size());
}

std::string GuardedPattern::textRepresentation() const {
  return fmt::format(FMT_STRING("({}) if {}"), pattern->textRepresentation(),
                     operand->textRepresentation());
}

} // namespace ir
} // namespace seq
