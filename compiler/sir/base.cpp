#include "base.h"
#include <sstream>

using namespace seq;
using namespace ir;

StringAttribute::StringAttribute(std::string value) : value{value} {}

std::string StringAttribute::textRepresentation() const {
  return "\"" + value + "\"";
}

BoolAttribute::BoolAttribute(bool value) : value{value} {}

std::string BoolAttribute::textRepresentation() const {
  return (value) ? "true" : "false";
}

AttributeHolder::AttributeHolder() : kvStore{} {}

std::shared_ptr<Attribute>
AttributeHolder::getAttribute(std::string key) const {
  auto found = kvStore.find(key);
  return (found != kvStore.end()) ? found->second
                                  : std::shared_ptr<Attribute>{nullptr};
}

void AttributeHolder::setAttribute(std::string key,
                                   std::shared_ptr<Attribute> value) {
  kvStore[key] = value;
}

std::string AttributeHolder::textRepresentation() const {
  std::stringstream stream;

  stream << "[";
  auto it = kvStore.begin();
  while (it != kvStore.end()) {
    it++;
    stream << it->first << "=" << it->second->textRepresentation()
           << ((it != kvStore.end()) ? ", " : "");
  }
  stream << "]";
  return stream.str();
}