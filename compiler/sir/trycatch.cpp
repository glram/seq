#include "util/fmt/format.h"

#include "bblock.h"
#include "trycatch.h"
#include "var.h"

namespace seq {
namespace ir {

int TryCatch::currentId = 0;

std::string TryCatch::referenceString() const {
  return fmt::format(FMT_STRING("try#{}"), id);
}

std::string TryCatch::textRepresentation() const {
  fmt::memory_buffer buf;
  fmt::format_to(buf, FMT_STRING("{}["), referenceString());
  for (auto it = children.begin(); it != children.end(); it++) {
    fmt::format_to(buf, FMT_STRING("{}"), (*it)->textRepresentation());
    if (it + 1 != children.end())
      fmt::format_to(buf, FMT_STRING(", "));
  }
  fmt::format_to(buf, FMT_STRING("]{{"));
  for (int i = 0; i < catchBlocks.size(); i++) {
    fmt::format_to(buf, FMT_STRING("{}: {}"), catchTypes[i]->getName(),
                   catchBlocks[i].lock()->referenceString());
    if (i + 1 != catchBlocks.size()) {
      fmt::format_to(buf, FMT_STRING(", "));
    }
  }
  buf.push_back('}');

  auto finally = finallyBlock.lock();
  if (finally)
    fmt::format_to(buf, FMT_STRING(" finally {}; {}"), finally->referenceString(),
                   attributeString());

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
