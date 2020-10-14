#include "base.h"
#include "bblock.h"
#include "trycatch.h"

#include <algorithm>

#include "util/fmt/ostream.h"

namespace seq {
namespace ir {

const std::string kSrcInfoAttribute = "srcInfoAttribute";
const std::string kLoopAttribute = "loopAttribute";
const std::string kFuncAttribute = "funcAttributes";

std::string StringAttribute::textRepresentation() const {
  return fmt::format(FMT_STRING("\"{}\""), value);
}

std::string BoolAttribute::textRepresentation() const {
  return fmt::format(FMT_STRING("{}"), value);
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

bool FuncAttribute::has(const std::string &val) const {
  return std::find(attributes.begin(), attributes.end(), val) != attributes.end();
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
