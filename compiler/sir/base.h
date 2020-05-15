#pragma once

#include <map>
#include <memory>
#include <string>

namespace seq {
namespace ir {

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

class AttributeHolder {
private:
  std::map<std::string, std::shared_ptr<Attribute>> kvStore;

public:
  AttributeHolder();

  virtual std::string textRepresentation() const;

  void setAttribute(std::string key, std::shared_ptr<Attribute> value);
  std::shared_ptr<Attribute> getAttribute(std::string key) const;
};
} // namespace ir
} // namespace seq