#include "util/fmt/format.h"

#include "operand.h"
#include "pattern.h"
#include "var.h"

#include "common/visitor.h"

namespace seq {
namespace ir {

int Pattern::currentId = 0;

void Pattern::resetId() { Pattern::currentId = 0; }

void Pattern::accept(common::SIRVisitor &v) { v.visit(getShared()); }

std::string Pattern::referenceString() const {
  return fmt::format(FMT_STRING("p#{}"), id);
}

WildcardPattern::WildcardPattern(std::shared_ptr<types::Type> type)
    : var(std::make_shared<Var>(type)) {}

void WildcardPattern::accept(common::SIRVisitor &v) {
  v.visit(getShared<WildcardPattern>());
}

std::string WildcardPattern::textRepresentation() const {
  return fmt::format(FMT_STRING("_#{}"), getId());
}

BoundPattern::BoundPattern(std::shared_ptr<Pattern> p,
                           std::shared_ptr<types::Type> type)
    : var(std::make_shared<Var>(std::move(type))), pattern(std::move(p)) {}

void BoundPattern::accept(common::SIRVisitor &v) { v.visit(getShared<BoundPattern>()); }

std::string BoundPattern::textRepresentation() const {
  return fmt::format(FMT_STRING("(({}->{})#{}"), pattern->textRepresentation(),
                     var->referenceString(), getId());
}

void StarPattern::accept(common::SIRVisitor &v) { v.visit(getShared<StarPattern>()); }

std::string StarPattern::textRepresentation() const {
  return fmt::format(FMT_STRING("...#{}"), getId());
}

void IntPattern::accept(common::SIRVisitor &v) { v.visit(getShared<IntPattern>()); }

std::string IntPattern::textRepresentation() const {
  return fmt::format(FMT_STRING("{}#{}"), value, getId());
}

void BoolPattern::accept(common::SIRVisitor &v) { v.visit(getShared<BoolPattern>()); }

std::string BoolPattern::textRepresentation() const {
  return fmt::format(FMT_STRING("{}#{}"), (value) ? "true" : "false", getId());
}

void StrPattern::accept(common::SIRVisitor &v) { v.visit(getShared<StrPattern>()); }

std::string StrPattern::textRepresentation() const {
  return fmt::format(FMT_STRING("'{}'#{}"), value, getId());
}

void SeqPattern::accept(common::SIRVisitor &v) { v.visit(getShared<SeqPattern>()); }

std::string SeqPattern::textRepresentation() const {
  return fmt::format(FMT_STRING("s'{}'#{}"), value, getId());
}

void RecordPattern::accept(common::SIRVisitor &v) {
  v.visit(getShared<RecordPattern>());
}

std::string RecordPattern::textRepresentation() const {
  fmt::memory_buffer buf;
  buf.push_back('{');
  for (auto it = patterns.begin(); it != patterns.end(); it++) {
    fmt::format_to(buf, FMT_STRING("({})"), (*it)->textRepresentation());
    if (it + 1 != patterns.end())
      fmt::format_to(buf, FMT_STRING(", "));
  }
  fmt::format_to(buf, FMT_STRING("}}#{}"), getId());
  return std::string(buf.data(), buf.size());
}

void ArrayPattern::accept(common::SIRVisitor &v) { v.visit(getShared<ArrayPattern>()); }

std::string ArrayPattern::textRepresentation() const {
  fmt::memory_buffer buf;
  buf.push_back('[');
  for (auto it = patterns.begin(); it != patterns.end(); it++) {
    fmt::format_to(buf, FMT_STRING("({})"), (*it)->textRepresentation());
    if (it + 1 != patterns.end())
      fmt::format_to(buf, FMT_STRING(", "));
  }
  fmt::format_to(buf, FMT_STRING("]#{}"), getId());
  return std::string(buf.data(), buf.size());
}

void OptionalPattern::accept(common::SIRVisitor &v) {
  v.visit(getShared<OptionalPattern>());
}

std::string OptionalPattern::textRepresentation() const {
  return fmt::format(FMT_STRING("{}?#{}"), pattern->textRepresentation(), getId());
}

void RangePattern::accept(common::SIRVisitor &v) { v.visit(getShared<RangePattern>()); }

std::string RangePattern::textRepresentation() const {
  return fmt::format(FMT_STRING("{}...{}#{}"), lower, higher, getId());
}

void OrPattern::accept(common::SIRVisitor &v) { v.visit(getShared<OrPattern>()); }

std::string OrPattern::textRepresentation() const {
  fmt::memory_buffer buf;
  for (auto it = patterns.begin(); it != patterns.end(); it++) {
    fmt::format_to(buf, FMT_STRING("({})"), (*it)->textRepresentation());
    if (it + 1 != patterns.end())
      fmt::format_to(buf, FMT_STRING(" or "));
  }
  fmt::format_to(buf, FMT_STRING("#{}"), getId());
  return std::string(buf.data(), buf.size());
}

void GuardedPattern::accept(common::SIRVisitor &v) {
  v.visit(getShared<GuardedPattern>());
}

std::string GuardedPattern::textRepresentation() const {
  return fmt::format(FMT_STRING("({}) if {}#{}"), pattern->textRepresentation(),
                     operand->textRepresentation(), getId());
}

} // namespace ir
} // namespace seq
