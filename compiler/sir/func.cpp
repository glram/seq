#include <algorithm>

#include "util/fmt/format.h"

#include "common/visitor.h"

#include "bblock.h"
#include "func.h"
#include "trycatch.h"
#include "var.h"

namespace seq {
namespace ir {

Func::Func(std::string name, std::vector<std::string> argNames,
           std::shared_ptr<types::Type> type)
    : Var(std::move(name), type, true), argNames(argNames), external(), generator(),
      internal(), llvmOnly() {

  argVars = std::vector<std::shared_ptr<Var>>{};
  auto argTypes = std::static_pointer_cast<types::FuncType>(type)->getArgTypes();
  for (int i = 0; i < argTypes.size(); i++) {
    argVars.push_back(std::make_shared<Var>(argNames[i], argTypes[i]));
  }
  blocks.push_back(std::make_shared<BasicBlock>());
}

void Func::setArgNames(std::vector<std::string> names) { argNames = names; }

void Func::setType(std::shared_ptr<types::Type> type) {
  Var::setType(type);
  argVars.clear();
  auto argTypes = std::static_pointer_cast<types::FuncType>(type)->getArgTypes();
  for (int i = 0; i < argTypes.size(); i++) {
    argVars.push_back(std::make_shared<Var>(argNames[i], argTypes[i]));
  }
}

std::shared_ptr<Var> Func::getArgVar(const std::string &name) {
  auto it = std::find(argNames.begin(), argNames.end(), name);
  return (it == argNames.end()) ? std::shared_ptr<Var>{}
                                : argVars[it - argNames.begin()];
}

std::string Func::textRepresentation() const {
  fmt::memory_buffer buf;
  fmt::format_to(buf, FMT_STRING("def {}(\n"), referenceString());
  for (auto &arg : argVars) {
    fmt::format_to(buf, FMT_STRING("{}\n"), arg->textRepresentation());
  }
  fmt::format_to(
      buf, FMT_STRING(") -> {} [\n"),
      std::static_pointer_cast<types::FuncType>(type)->getRType()->referenceString());
  for (const auto &var : vars) {
    fmt::format_to(buf, FMT_STRING("{}\n"), var->textRepresentation());
  }

  fmt::format_to(buf, FMT_STRING("]{{\n"));
  if (llvmOnly) {
    fmt::format_to(buf, FMT_STRING("internal: {}.{}\n"), parent->referenceString(),
                   magicName);
  } else if (external) {
    fmt::format_to(buf, FMT_STRING("external\n"));
  } else {
    for (const auto &block : blocks) {
      fmt::format_to(buf, FMT_STRING("{}\n"), block->textRepresentation());
    }
  }
  if (tc)
    fmt::format_to(buf, FMT_STRING("{}\n"), tc->textRepresentation());
  fmt::format_to(buf, FMT_STRING("}}; {}"), attributeString());
  return std::string(buf.data(), buf.size());
}

std::string Func::referenceString() const {
  return fmt::format(FMT_STRING("f${}#{}"), name, id);
}
void Func::accept(common::IRVisitor &v) {
  v.visit(std::static_pointer_cast<Func>(getShared()));
}

} // namespace ir
} // namespace seq
