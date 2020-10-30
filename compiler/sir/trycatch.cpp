#include "trycatch.h"

#include "util/fmt/format.h"

#include "bblock.h"
#include "var.h"

#include "common/visitor.h"

namespace seq {
namespace ir {

int TryCatch::currentId = 0;

TryCatch::TryCatch() : id(currentId++) {
  flagVar = std::make_shared<Var>(types::kIntType);
}

void TryCatch::resetId() { currentId = 0; }

void TryCatch::accept(common::IRVisitor &v) { v.visit(getShared()); }

void TryCatch::addCatch(std::shared_ptr<types::Type> catchType, std::string name,
                        std::weak_ptr<BasicBlock> handler) {
  catchTypes.push_back(catchType);
  catchBlocks.push_back(handler);
  if (!name.empty()) {
    catchVars.push_back(std::make_shared<Var>(name, catchType));
  } else {
    catchVars.push_back(nullptr);
  }
  catchVarNames.push_back(name);
}

std::vector<std::shared_ptr<TryCatch>>
TryCatch::getPath(std::shared_ptr<TryCatch> dst) {
  auto cur = getShared();
  std::vector<std::shared_ptr<TryCatch>> ret = {};
  for (; cur; cur = cur->getParent().lock()) {
    if (dst && cur->getId() == dst->getId())
      return ret;
    ret.push_back(cur);
  }
  if (!dst) {
    if (cur)
      ret.push_back(cur);
    return ret;
  }

  return {};
}

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
    fmt::format_to(buf, FMT_STRING("{}{}{}: {}\n"),
                   catchTypes[i] ? catchTypes[i]->referenceString() : "*",
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

} // namespace ir
} // namespace seq
