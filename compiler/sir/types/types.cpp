#include <algorithm>

#include "types.h"
#include "util/fmt/format.h"

namespace seq {
namespace ir {
namespace types {

Optional::Optional(std::shared_ptr<Type> base)
    : Type(fmt::format(FMT_STRING("Optional[{}]"), base->getName())),
      base(std::move(base)) {}

Array::Array(std::shared_ptr<Type> base)
    : Type(fmt::format(FMT_STRING("Array[{}]"), base->getName())),
      base(std::move(base)) {}

Pointer::Pointer(std::shared_ptr<Type> base)
    : Type(fmt::format(FMT_STRING("Pointer[{}]"), base->getName())),
      base(std::move(base)) {}

Reference::Reference(std::shared_ptr<Type> base)
    : Type(fmt::format(FMT_STRING("Ref[{}]"), base->getName())),
      base(std::move(base)) {}

Generator::Generator(std::shared_ptr<Type> base)
    : Type(fmt::format(FMT_STRING("Generator[{}]"), base->getName())),
      base(std::move(base)) {}

} // namespace types
} // namespace ir
} // namespace seq