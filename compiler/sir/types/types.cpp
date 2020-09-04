#include <algorithm>

#include "types.h"
#include "util/fmt/format.h"

namespace seq {
namespace ir {
namespace types {

const std::shared_ptr<Type> kStringType = std::make_shared<Type>("str");
const std::shared_ptr<Type> kBoolType = std::make_shared<Type>("bool");
const std::shared_ptr<Type> kSeqType = std::make_shared<Type>("seq");
const std::shared_ptr<Type> kFloatType = std::make_shared<Type>("float");
const std::shared_ptr<Type> kIntType = std::make_shared<Type>("int");
const std::shared_ptr<Type> kUIntType = std::make_shared<Type>("uint");
const std::shared_ptr<Type> kAnyType = std::make_shared<Type>("any");
const std::shared_ptr<Type> kVoidType = std::make_shared<Type>("void");
const std::shared_ptr<Type> kByteType = std::make_shared<Type>("byte");

const std::shared_ptr<Type> kNoArgVoidFuncType = std::make_shared<FuncType>(
    "void->void", kVoidType, std::vector<std::shared_ptr<Type>>());

std::shared_ptr<Type> RecordType::getMemberType(std::string n) {
  auto it = std::find(memberNames.begin(), memberNames.end(), n);
  return it == memberNames.end()? nullptr : memberTypes[it - memberNames.begin()];
}

std::string RecordType::textRepresentation() const {
  fmt::memory_buffer buf;
  fmt::format_to(buf, FMT_STRING("{}: ("), getName());
  for (auto i = 0; i < memberNames.size(); ++i) {
    auto sep = i + 1 != memberNames.size()? ", " : "";
    fmt::format_to(buf, FMT_STRING("{}: {}{}"), memberNames[i], memberTypes[i]->referenceString(), sep);
  }
  buf.push_back(')');
  return std::string(buf.data(), buf.size());
}

std::string FuncType::textRepresentation() const {
  fmt::memory_buffer buf;
  fmt::format_to(buf, FMT_STRING("{}: ("), getName());
  for (auto it = argTypes.begin(); it != argTypes.end(); ++it) {
    auto sep = it + 1 != argTypes.end()? ", " : "";
    fmt::format_to(buf, FMT_STRING("{}{}"), (*it)->referenceString(), sep);
  }
  fmt::format_to(buf, FMT_STRING(")->{}"), rType->referenceString());
  return std::string(buf.data(), buf.size());
}

std::string PartialFuncType::textRepresentation() const {
  fmt::memory_buffer buf;
  fmt::format_to(buf, FMT_STRING("{}: ({}, ["), getName(), callee->referenceString());
  for (auto it = callTypes.begin(); it != callTypes.end(); ++it) {
    auto sep = it + 1 != callTypes.end()? ", " : "";
    fmt::format_to(buf, FMT_STRING("{}{}"), (*it)->referenceString(), sep);
  }
  buf.push_back(']');
  return std::string(buf.data(), buf.size());
}

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