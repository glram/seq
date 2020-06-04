#pragma once

#include <map>
#include <memory>
#include <string>

#include "util/common.h"

namespace seq {
namespace ir {

class TryCatch;

static const std::string kSrcInfoAttribute = "srcInfo";

class Attribute {
public:
  virtual std::string textRepresentation() const = 0;
};

class StringAttribute : public Attribute {
private:
  std::string value;

public:
  explicit StringAttribute(std::string value);

  std::string textRepresentation() const override;
};

class BoolAttribute : public Attribute {
private:
  bool value;

public:
  explicit BoolAttribute(bool value);

  std::string textRepresentation() const override;
};

class TryCatchAttribute : public Attribute {
private:
  std::weak_ptr<TryCatch> handler;

public:
  explicit TryCatchAttribute(std::weak_ptr<TryCatch> handler);
  std::string textRepresentation() const override;
};

class SrcInfoAttribute : public Attribute {
private:
  seq::SrcInfo info;

public:
  explicit SrcInfoAttribute(seq::SrcInfo info);

  seq::SrcInfo getInfo();
  std::string textRepresentation() const override;
};

template <typename A>
class AttributeHolder : public std::enable_shared_from_this<A> {
private:
  std::map<std::string, std::shared_ptr<Attribute>> kvStore;

public:
  AttributeHolder();

  virtual std::string textRepresentation() const = 0;
  virtual std::string referenceString() const = 0;
  std::string attributeString() const;

  void setAttribute(std::string key, std::shared_ptr<Attribute> value);
  std::shared_ptr<Attribute> getAttribute(std::string key) const;

  std::shared_ptr<A> getShared() const;
};
} // namespace ir
} // namespace seq