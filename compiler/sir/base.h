#pragma once

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "util/common.h"
#include "util/fmt/format.h"

namespace seq {
namespace ir {

class TryCatch;
class BasicBlock;

extern const std::string kSrcInfoAttribute;
extern const std::string kTryCatchAttribute;
extern const std::string kLoopAttribute;
extern const std::string kFuncAttribute;

struct Attribute {
  virtual std::string textRepresentation() const = 0;
};

struct StringAttribute : public Attribute {
  std::string value;

  explicit StringAttribute(std::string value) : value(std::move(value)) {}
  std::string textRepresentation() const override;
};

struct BoolAttribute : public Attribute {
  bool value;

  explicit BoolAttribute(bool value) : value(value) {}
  std::string textRepresentation() const override;
};

struct TryCatchAttribute : public Attribute {
  std::shared_ptr<TryCatch> handler;

  explicit TryCatchAttribute(std::shared_ptr<TryCatch> handler)
      : handler(std::move(handler)) {}
  std::string textRepresentation() const override;
};

struct LoopAttribute : public Attribute {
  std::weak_ptr<BasicBlock> setup;
  std::weak_ptr<BasicBlock> cond;
  std::weak_ptr<BasicBlock> begin;
  std::weak_ptr<BasicBlock> update;
  std::weak_ptr<BasicBlock> end;

  LoopAttribute(std::weak_ptr<BasicBlock> setup, std::weak_ptr<BasicBlock> cond,
                std::weak_ptr<BasicBlock> begin,
                std::weak_ptr<BasicBlock> update, std::weak_ptr<BasicBlock> end)
      : setup(std::move(setup)), cond(std::move(cond)), begin(std::move(begin)),
        update(std::move(update)), end(std::move(end)) {}
  std::string textRepresentation() const override;
};

struct SrcInfoAttribute : public Attribute {
  seq::SrcInfo info;

  explicit SrcInfoAttribute(seq::SrcInfo info) : info(std::move(info)) {}
  std::string textRepresentation() const override;
};

struct FuncAttribute : public Attribute {
  std::vector<std::string> attributes;

  explicit FuncAttribute(std::vector<std::string> attributes)
      : attributes(std::move(attributes)) {}
  std::string textRepresentation() const override;
};

template <typename A>
class AttributeHolder : public std::enable_shared_from_this<A> {
private:
  std::map<std::string, std::shared_ptr<Attribute>> kvStore;

public:
  virtual ~AttributeHolder() = default;

  virtual std::string textRepresentation() const = 0;
  virtual std::string referenceString() const = 0;
  std::string attributeString() const {
    fmt::memory_buffer buf;
    buf.push_back('{');
    for (auto &it : kvStore) {
      fmt::format_to(buf, FMT_STRING("{}: {}, "), it.first,
                     it.second->textRepresentation());
    }
    buf.push_back('}');
    return std::string(buf.data(), buf.size());
  }

  void setAttribute(const std::string &key, std::shared_ptr<Attribute> value) {
    kvStore[key] = std::move(value);
  }

  std::shared_ptr<Attribute> getAttribute(const std::string &key) {
    return kvStore[key];
  }

  std::shared_ptr<A> getShared() {
    return std::static_pointer_cast<A>(this->shared_from_this());
  }
};

} // namespace ir
} // namespace seq
