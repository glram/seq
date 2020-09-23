#include <algorithm>
#include <utility>

#include "types.h"
#include "util/fmt/format.h"

namespace {
using namespace seq::ir::types;
std::vector<std::shared_ptr<Type>>
resolvePartials(std::vector<std::shared_ptr<Type>> calleeArgs,
                std::vector<std::shared_ptr<Type>> partials) {
  std::vector<std::shared_ptr<Type>> ret;
  for (auto it = partials.begin(); it != partials.end(); ++it) {
    if (!*it)
      ret.push_back(calleeArgs[it - partials.begin()]);
  }
  return ret;
}
} // namespace

namespace seq {
namespace ir {
namespace types {

int Type::currentId = 0;

std::shared_ptr<Type> kStringType = std::make_shared<Type>("str");
std::shared_ptr<Type> kBoolType = std::make_shared<Type>("bool");
std::shared_ptr<Type> kSeqType = std::make_shared<Type>("seq");
std::shared_ptr<Type> kFloatType = std::make_shared<Type>("float");
std::shared_ptr<Type> kIntType = std::make_shared<Type>("int");
std::shared_ptr<Type> kAnyType = std::make_shared<Type>("any");
std::shared_ptr<Type> kVoidType = std::make_shared<Type>("void");
std::shared_ptr<Type> kByteType = std::make_shared<Type>("byte");
std::shared_ptr<Type> kNoArgVoidFuncType = std::make_shared<FuncType>(
    "void->void", kVoidType, std::vector<std::shared_ptr<Type>>());

void Type::resetId() {
  currentId = 0;
  kStringType = std::make_shared<Type>("str");
  kBoolType = std::make_shared<Type>("bool");
  kSeqType = std::make_shared<Type>("seq");
  kFloatType = std::make_shared<Type>("float");
  kIntType = std::make_shared<Type>("int");
  kAnyType = std::make_shared<Type>("any");
  kVoidType = std::make_shared<Type>("void");
  kByteType = std::make_shared<Type>("byte");
  kNoArgVoidFuncType = std::make_shared<FuncType>("void->void", kVoidType,
                                                  std::vector<std::shared_ptr<Type>>());
}

std::string Type::referenceString() const {
  return fmt::format(FMT_STRING("{}#{}"), name, id);
}

std::shared_ptr<Type> RecordType::getMemberType(std::string n) {
  auto it = std::find(memberNames.begin(), memberNames.end(), n);
  return it == memberNames.end() ? nullptr : memberTypes[it - memberNames.begin()];
}

std::string RecordType::textRepresentation() const {
  fmt::memory_buffer buf;
  fmt::format_to(buf, FMT_STRING("{}: ("), referenceString());
  for (auto i = 0; i < memberNames.size(); ++i) {
    auto sep = i + 1 != memberNames.size() ? ", " : "";
    fmt::format_to(buf, FMT_STRING("{}: {}{}"), memberNames[i],
                   memberTypes[i]->referenceString(), sep);
  }
  buf.push_back(')');
  return std::string(buf.data(), buf.size());
}

std::string RefType::textRepresentation() const {
  return fmt::format("{}: ref({})", referenceString(), contents->textRepresentation());
}

std::string FuncType::textRepresentation() const {
  fmt::memory_buffer buf;
  fmt::format_to(buf, FMT_STRING("{}: ("), referenceString());
  for (auto it = argTypes.begin(); it != argTypes.end(); ++it) {
    auto sep = it + 1 != argTypes.end() ? ", " : "";
    fmt::format_to(buf, FMT_STRING("{}{}"), (*it)->referenceString(), sep);
  }
  fmt::format_to(buf, FMT_STRING(")->{}"), rType->referenceString());
  return std::string(buf.data(), buf.size());
}

PartialFuncType::PartialFuncType(std::string name, std::shared_ptr<FuncType> callee,
                                 std::vector<std::shared_ptr<Type>> callTypes)

    : FuncType(std::move(name), callee->getRType(),
               resolvePartials(callee->getArgTypes(), callTypes)),
      callee(std::move(callee)), callTypes(std::move(callTypes)) {}

std::string PartialFuncType::textRepresentation() const {
  fmt::memory_buffer buf;
  fmt::format_to(buf, FMT_STRING("{}: ({}, ["), referenceString(),
                 callee->referenceString());
  for (auto it = callTypes.begin(); it != callTypes.end(); ++it) {
    auto sep = it + 1 != callTypes.end() ? ", " : "";
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

Generator::Generator(std::shared_ptr<Type> base)
    : Type(fmt::format(FMT_STRING("Generator[{}]"), base->getName())),
      base(std::move(base)) {}

IntNType::IntNType(unsigned int len, bool sign)
    : Type(fmt::format(FMT_STRING("{}Int{}"), sign ? "" : "U", len)), len(len),
      sign(sign) {}

} // namespace types
} // namespace ir
} // namespace seq