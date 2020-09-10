#include "util/fmt/format.h"

#include "bblock.h"
#include "func.h"
#include "module.h"
#include "trycatch.h"
#include "var.h"

namespace seq {
namespace ir {

IRModule::IRModule(std::string name)
    : name(std::move(name)),
      baseFunc(std::make_shared<Func>("base", std::vector<std::string>(),
                                      types::kNoArgVoidFuncType)),
      argVar(std::make_shared<Var>(name + "-argv", types::kIntType)),
      tc(std::make_shared<TryCatch>()) {}

void IRModule::addGlobal(std::shared_ptr<Var> var) {
  var->setModule(getShared());
  globals.push_back(var);
}

std::string IRModule::textRepresentation() const {
  fmt::memory_buffer buf;
  fmt::format_to(buf, FMT_STRING("module {}{{\n"), name);
  fmt::format_to(buf, "{}\n", baseFunc->textRepresentation());

  for (const auto &global : globals) {
    fmt::format_to(buf, FMT_STRING("{}\n"), global->textRepresentation());
  }
  fmt::format_to(buf, FMT_STRING("}}; {}"), attributeString());
  fmt::format_to(buf, FMT_STRING("\n\n{}"), tc->textRepresentation());
  return std::string(buf.data(), buf.size());
}

} // namespace ir
} // namespace seq
