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
extern const std::string kLoopAttribute;
extern const std::string kFuncAttribute;

/// Base for SIR attributes.
struct Attribute {
  virtual ~Attribute() = default;

  /// @return text representation of the attribute
  virtual std::string textRepresentation() const = 0;
};

/// Attribute containing information about loops.
struct LoopAttribute : public Attribute {
  /// loop setup block
  std::weak_ptr<BasicBlock> setup;
  /// loop condition block
  std::weak_ptr<BasicBlock> cond;
  /// loop beginning block
  std::weak_ptr<BasicBlock> begin;
  /// loop update block
  std::weak_ptr<BasicBlock> update;
  /// loop end block
  std::weak_ptr<BasicBlock> end;

  /// Constructs a LoopAttribute.
  /// @param setup the setup block, nullptr if doesn't exist
  /// @param cond the condition block, nullptr if doesn't exist
  /// @param begin the begin block, nullptr if doesn't exist
  /// @param update the update block, nullptr if doesn't exist
  /// @param end the end block, nullptr if doesn't exist
  LoopAttribute(std::weak_ptr<BasicBlock> setup, std::weak_ptr<BasicBlock> cond,
                std::weak_ptr<BasicBlock> begin, std::weak_ptr<BasicBlock> update,
                std::weak_ptr<BasicBlock> end)
      : setup(std::move(setup)), cond(std::move(cond)), begin(std::move(begin)),
        update(std::move(update)), end(std::move(end)) {}
  std::string textRepresentation() const override;
};

/// Attribute containing SrcInfo
struct SrcInfoAttribute : public Attribute {
  /// source info
  seq::SrcInfo info;

  /// Constructs a SrcInfoAttribute.
  /// @param info the source info
  explicit SrcInfoAttribute(seq::SrcInfo info) : info(std::move(info)) {}
  std::string textRepresentation() const override;
};

/// Attribute containing function information
struct FuncAttribute : public Attribute {
  /// attributes map
  std::map<std::string, std::string> attributes;

  /// Constructs a FuncAttribute.
  /// @param attributes the map of attributes
  explicit FuncAttribute(std::map<std::string, std::string> attributes)
      : attributes(std::move(attributes)) {}

  /// @return true if the map contains val, false otherwise
  bool has(const std::string &val) const;
  std::string textRepresentation() const override;
};

/// CRTP base of all SIR objects.
template <typename A> class AttributeHolder : public std::enable_shared_from_this<A> {
private:
  /// key-value attribute store
  std::map<std::string, std::shared_ptr<Attribute>> kvStore;

public:
  virtual ~AttributeHolder() = default;

  /// @return a text representation of the object's attributes
  std::string attributeString() const {
    fmt::memory_buffer buf;
    buf.push_back('[');

    for (auto it = kvStore.begin(); it != kvStore.end(); it++) {
      if (it != kvStore.begin())
        fmt::format_to(buf, FMT_STRING(", "));

      fmt::format_to(buf, FMT_STRING("({}, {})"), it->first,
                     it->second->textRepresentation());
    }
    buf.push_back(']');
    return std::string(buf.data(), buf.size());
  }

  /// Sets an attribute
  /// @param key the attribute's key
  /// @param value the attribute
  void setAttribute(const std::string &key, std::shared_ptr<Attribute> value) {
    kvStore[key] = std::move(value);
  }

  /// Gets an attribute static casted to the desired type.
  /// @param key the key
  /// @tparam AttributeType the return type
  template <typename AttributeType = Attribute>
  std::shared_ptr<AttributeType> getAttribute(const std::string &key) {
    auto it = kvStore.find(key);
    return it == kvStore.end() ? nullptr
                               : std::static_pointer_cast<AttributeType>(it->second);
  }

  /// Gets a shared pointer to the object.
  /// @tparam ReturnType the return type.
  template <typename ReturnType = A> std::shared_ptr<ReturnType> getShared() {
    return std::static_pointer_cast<ReturnType>(this->shared_from_this());
  }

  /// @tparam ReturnType the desired type.
  template <typename ReturnType> std::shared_ptr<ReturnType> as() {
    return getShared<ReturnType>();
  }

  /// @return a text representation of a reference to the object
  virtual std::string referenceString() const = 0;

  /// @return a text representation of a definition of the object
  virtual std::string textRepresentation() const = 0;
};

} // namespace ir
} // namespace seq
