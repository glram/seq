#pragma once

#include <map>
#include <memory>
#include <string>

#include "util/common.h"
#include "util/fmt/format.h"

namespace seq {
namespace ir {

class TryCatch;

static const std::string kSrcInfoAttribute = "srcInfo";

struct Attribute {
  virtual std::string textRepresentation() const = 0;
};

struct StringAttribute : public Attribute {
  std::string value;

  std::string textRepresentation() const override;
};

struct BoolAttribute : public Attribute {
  bool value;

  std::string textRepresentation() const override;
};

struct TryCatchAttribute : public Attribute {
  std::weak_ptr<TryCatch> handler;

  std::string textRepresentation() const override;
};

struct SrcInfoAttribute : public Attribute {
  seq::SrcInfo info;

  std::string textRepresentation() const override;
};

template <typename A>
class AttributeHolder : public std::enable_shared_from_this<A> {
private:
  std::map<std::string, std::shared_ptr<Attribute>> kvStore;

public:
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
    return std::static_pointer_cast<A>(shared_from_this());
  }
};

} // namespace ir
} // namespace seq
