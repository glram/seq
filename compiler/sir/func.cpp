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
      internal(), builtin() {

  argVars = std::vector<std::shared_ptr<Var>>{};
  auto argTypes = type->as<types::FuncType>()->getArgTypes();
  for (int i = 0; i < argTypes.size(); i++) {
    argVars.push_back(std::make_shared<Var>(argNames[i], argTypes[i]));
  }
  blocks.push_back(std::make_shared<BasicBlock>("sir_start"));
}

void Func::accept(common::SIRVisitor &v) { v.visit(getShared<Func>()); }

void Func::realize(std::shared_ptr<types::FuncType> type,
                   std::vector<std::string> names) {
  Var::setType(type);
  argVars.clear();
  auto argTypes = std::static_pointer_cast<types::FuncType>(type)->getArgTypes();
  for (int i = 0; i < argTypes.size(); i++) {
    argVars.push_back(std::make_shared<Var>(names[i], argTypes[i]));
  }
  argNames = std::move(names);
}

std::shared_ptr<Var> Func::getArgVar(const std::string &name) {
  auto it = std::find(argNames.begin(), argNames.end(), name);
  return (it == argNames.end()) ? std::shared_ptr<Var>{}
                                : argVars[it - argNames.begin()];
}

std::string Func::referenceString() const {
  return fmt::format(FMT_STRING("{}.fn_{}"), name, id);
}

std::string Func::textRepresentation() const {
  fmt::memory_buffer buf;
  fmt::format_to(buf, FMT_STRING("def {}(\n"), referenceString());
  for (const auto & argVar : argVars) {
    fmt::format_to(buf, FMT_STRING("    {}\n"), argVar->textRepresentation());
  }
  fmt::format_to(
      buf, FMT_STRING(") -> {} [\n"),
      type->as<types::FuncType>()->getRType()->referenceString());

  for (const auto & var : vars) {
    fmt::format_to(buf, FMT_STRING("    {}\n"), var->textRepresentation());
  }

  fmt::format_to(buf, FMT_STRING("]{{\n"));

  if (internal) {
    fmt::format_to(buf, FMT_STRING("    internal: {}.{}\n"), parent->referenceString(),
                   unmangledName);
  } else if (external) {
    fmt::format_to(buf, FMT_STRING("    external\n"));
  } else {
    for (const auto &block : blocks) {
      fmt::format_to(buf, FMT_STRING("{}\n"), block->textRepresentation());
    }
  }

  for (auto &tc : tryCatches) {
    fmt::format_to(buf, FMT_STRING("{}\n"), tc->textRepresentation());
  }

  fmt::format_to(buf, FMT_STRING("}}; {}"), attributeString());
  return std::string(buf.data(), buf.size());
}

} // namespace ir
} // namespace seq
