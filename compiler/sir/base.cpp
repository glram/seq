#include "base.h"
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
  return fmt::format(FMT_STRING("{}"), handler.lock()->referenceString());
}

std::string SrcInfoAttribute::textRepresentation() const {
  return fmt::format(FMT_STRING("{}"), info);
}

} // namespace ir
} // namespace seq
