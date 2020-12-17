#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "util/fmt/format.h"
#include "util/fmt/ostream.h"

#include "attribute.h"
#include "util/visitor.h"

namespace seq {
namespace ir {

class IRModule;

/// Mixin class for IR nodes that need ids.
class IdMixin {
private:
  /// the global id counter
  static int currentId;
  /// the instance's id
  int id;

public:
  /// Resets the global id counter.
  static void resetId();

  IdMixin() : id(currentId++) {}
  virtual ~IdMixin() = default;

  /// @return the node's id.
  int getId() const { return id; }
};

/// Base for named IR nodes.
class IRNode {
private:
  /// the node's name
  std::string name;
  /// key-value attribute store
  std::map<std::string, AttributePtr> kvStore;
  /// the module
  IRModule *module = nullptr;

public:
  /// Constructs a node.
  /// @param name the node's name
  explicit IRNode(std::string name = "") : name(std::move(name)) {}

  virtual ~IRNode() = default;

  /// @return the node's name
  const std::string &getName() const { return name; }
  /// Sets the node's name
  /// @param n the new name
  void setName(std::string n) { name = std::move(n); }

  /// Accepts visitors.
  /// @param v the visitor
  virtual void accept(util::SIRVisitor &v) = 0;

  /// Sets an attribute
  /// @param key the attribute's key
  /// @param value the attribute
  void setAttribute(const std::string &key, AttributePtr value) {
    kvStore[key] = std::move(value);
  }

  /// @return true if the key is in the store
  bool hasAttribute(const std::string &key) const {
    return kvStore.find(key) != kvStore.end();
  }

  /// Gets an attribute static casted to the desired type.
  /// @param key the key
  /// @tparam AttributeType the return type
  template <typename AttributeType = Attribute>
  AttributeType *getAttribute(const std::string &key) const {
    auto it = kvStore.find(key);
    return it != kvStore.end() ? static_cast<AttributeType *>(it->second.get())
                               : nullptr;
  }

  /// Helper to add source information.
  /// @param the source information
  void setSrcInfo(seq::SrcInfo s) {
    setAttribute(kSrcInfoAttribute, std::make_unique<SrcInfoAttribute>(std::move(s)));
  }

  /// @return a text representation of a reference to the object
  virtual std::string referenceString() const { return name; }

  friend std::ostream &operator<<(std::ostream &os, const IRNode &a) {
    return a.doFormat(os);
  }

  /// @return the IR module
  IRModule *getModule() const { return module; }
  /// Sets the module.
  /// @param m the new module
  void setModule(IRModule *m) { module = m; }

private:
  virtual std::ostream &doFormat(std::ostream &os) const = 0;
};

} // namespace ir
} // namespace seq

namespace fmt {
using seq::ir::IRNode;

template <typename Char>
struct formatter<IRNode, Char> : fmt::v6::internal::fallback_formatter<IRNode, Char> {};

} // namespace fmt