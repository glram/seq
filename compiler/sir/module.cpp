#include "util/fmt/format.h"

#include "bblock.h"
#include "func.h"
#include "module.h"
#include "var.h"

namespace seq {
namespace ir {

IRModule::IRModule(std::string name)
    : name(std::move(name)),
      baseFunc(std::make_shared<Func>("base", std::vector<std::string>(),
                                      types::kNoArgVoidFuncType)) {
  baseFunc->addBlock(std::make_shared<BasicBlock>());
}

std::string IRModule::textRepresentation() const {
  fmt::memory_buffer buf;
  fmt::format_to(buf, FMT_STRING("module {}{{\n"), name);
  for (const auto &global : globals) {
    fmt::format_to(buf, FMT_STRING("{}\n"), global->textRepresentation());
  }
  fmt::format_to(buf, FMT_STRING("}}; {}"), attributeString());
  return std::string(buf.data(), buf.size());
}

} // namespace ir
} // namespace seq
