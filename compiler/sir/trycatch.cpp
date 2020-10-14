#include "trycatch.h"

#include "util/fmt/format.h"

#include "bblock.h"
#include "var.h"

#include "common/visitor.h"

namespace seq {
namespace ir {

int TryCatch::currentId = 0;

void TryCatch::resetId() { currentId = 0; }

void TryCatch::accept(common::IRVisitor &v) { v.visit(getShared()); }

std::string TryCatch::referenceString() const {
  return fmt::format(FMT_STRING("try#{}"), id);
}

std::string TryCatch::textRepresentation() const {
  fmt::memory_buffer buf;
  fmt::format_to(buf, FMT_STRING("{}[\n"), referenceString());
  for (const auto &it : children) {
    fmt::format_to(buf, FMT_STRING("{}\n"), it->textRepresentation());
  }
  fmt::format_to(buf, FMT_STRING("]{{\n"));
  for (int i = 0; i < catchBlocks.size(); i++) {
    fmt::format_to(buf, FMT_STRING("{}{}{}: {}\n"), catchTypes[i]->referenceString(),
                   catchVars[i] ? " -> " : "",
                   catchVars[i] ? catchVars[i]->referenceString() : "",
                   catchBlocks[i].lock()->referenceString());
  }
  buf.push_back('}');

  auto finally = finallyBlock.lock();
  if (finally)
    fmt::format_to(buf, FMT_STRING("\nfinally {}"), finally->referenceString());

  fmt::format_to(buf, FMT_STRING("; {}"), attributeString());

  return std::string(buf.data(), buf.size());
}
void TryCatch::addCatch(std::shared_ptr<types::Type> catchType, std::string name,
                        std::weak_ptr<BasicBlock> handler) {
  catchTypes.push_back(catchType);
  catchBlocks.push_back(handler);
  catchVars.push_back(std::make_shared<Var>(name, catchType));
  catchVarNames.push_back(name);
}

} // namespace ir
} // namespace seq
