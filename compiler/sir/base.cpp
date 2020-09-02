#include "base.h"
#include "bblock.h"
#include "trycatch.h"

#include "util/fmt/ostream.h"

namespace seq {
namespace ir {

std::string StringAttribute::textRepresentation() const {
  return fmt::format(FMT_STRING("\"{}\""), value);
}

std::string BoolAttribute::textRepresentation() const {
  return fmt::format(FMT_STRING("{}"), value);
}

std::string TryCatchAttribute::textRepresentation() const {
  return fmt::format(FMT_STRING("{}"), handler->referenceString());
}

std::string LoopAttribute::textRepresentation() const {
  auto setupStr = setup.lock() ? setup.lock()->referenceString() : "none";
  auto condStr = cond.lock() ? cond.lock()->referenceString() : "none";
  auto beginStr = begin.lock() ? begin.lock()->referenceString() : "none";
  auto updateStr = update.lock() ? update.lock()->referenceString() : "none";
  auto endStr = end.lock() ? end.lock()->referenceString() : "none";
  return fmt::format(FMT_STRING("loop({}, {}, {}, {}, {})"), setupStr, condStr,
                     beginStr, updateStr, endStr);
}

std::string FuncAttribute::textRepresentation() const {
  return fmt::format(FMT_STRING("{}"),
                     fmt::join(attributes.begin(), attributes.end(), ","));
}

std::string SrcInfoAttribute::textRepresentation() const {
  return fmt::format(FMT_STRING("{}"), info);
}

} // namespace ir
} // namespace seq
