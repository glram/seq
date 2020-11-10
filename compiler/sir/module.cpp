#include "util/fmt/format.h"

#include "bblock.h"
#include "func.h"
#include "module.h"
#include "trycatch.h"
#include "var.h"

#include "common/visitor.h"

namespace seq {
namespace ir {

SIRModule::SIRModule(std::string name)
    : name(std::move(name)),
      mainFunc(std::make_shared<Func>("base", std::vector<std::string>(),
                                      types::kNoArgVoidFuncType)),
      argVar(std::make_shared<Var>("argv", types::kStringArrayType)) {
  argVar->setGlobal();
  globals.push_back(argVar);
}

void SIRModule::accept(common::SIRVisitor &v) { v.visit(getShared()); }

void SIRModule::addGlobal(std::shared_ptr<Var> var) {
  var->setGlobal();
  globals.push_back(var);
}

std::string SIRModule::textRepresentation() const {
  fmt::memory_buffer buf;
  fmt::format_to(buf, FMT_STRING("module {}{{\n"), name);
  fmt::format_to(buf, "{}\n", mainFunc->textRepresentation());

  for (const auto &global : globals) {
    fmt::format_to(buf, FMT_STRING("{}\n"), global->textRepresentation());
  }
  fmt::format_to(buf, FMT_STRING("}}; {}"), attributeString());
  return std::string(buf.data(), buf.size());
}

} // namespace ir
} // namespace seq
