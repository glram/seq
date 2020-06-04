#include <sstream>

#include "base.h"
#include "trycatch.h"

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

TryCatchAttribute::TryCatchAttribute(std::weak_ptr<TryCatch> handler)
    : handler{handler} {}

std::string TryCatchAttribute::textRepresentation() const {
  auto locked = handler.lock();
  return "try#" + std::to_string(locked->getId());
}

template <typename A> AttributeHolder<A>::AttributeHolder() : kvStore{} {}

template <typename A>
std::shared_ptr<Attribute>
AttributeHolder<A>::getAttribute(std::string key) const {
  auto found = kvStore.find(key);
  return (found != kvStore.end()) ? found->second
                                  : std::shared_ptr<Attribute>{nullptr};
}

template <typename A>
void AttributeHolder<A>::setAttribute(std::string key,
                                      std::shared_ptr<Attribute> value) {
  kvStore[key] = value;
}

template <typename A> std::string AttributeHolder<A>::attributeString() const {
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

template <typename A> std::shared_ptr<A> AttributeHolder<A>::getShared() const {
  return this->shared_from_this();
}

SrcInfoAttribute::SrcInfoAttribute(seq::SrcInfo info) : info{info} {}

seq::SrcInfo SrcInfoAttribute::getInfo() { return info; }

std::string SrcInfoAttribute::textRepresentation() const {
  return "<srcinfo>>";
}
