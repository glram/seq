#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base.h"
#include "types/types.h"
#include "util/common.h"

#include "codegen/codegen.h"

namespace seq {
namespace ir {

class IRModule;

class Var : public AttributeHolder<Var> {
private:
  static int currentId;

protected:
  std::string name;
  std::shared_ptr<types::Type> type;
  std::weak_ptr<IRModule> module;

  bool global;

  int id;

public:
  Var(std::string name, std::shared_ptr<types::Type> type, bool global = false)
      : name(std::move(name)), type(std::move(type)), global(global), id(currentId++){};
  explicit Var(std::shared_ptr<types::Type> type) : Var("", std::move(type)) {}

  virtual void accept(codegen::CodegenVisitor &v, const std::string &nameOverride) { v.visit(getShared(), nameOverride); }

  static void resetId();

  virtual ~Var() = default;

  virtual void setType(std::shared_ptr<types::Type> type) {
    this->type = std::move(type);
  }

  void setName(std::string n) { name = n; }
  std::string getName() const { return name; }

  void setModule(std::weak_ptr<IRModule> m) { module = std::move(m); }
  std::weak_ptr<IRModule> getModule() { return module; }

  std::shared_ptr<types::Type> getType() { return type; }
  int getId() const { return id; }

  void setGlobal() { global = true; }
  bool isGlobal() const { return global; }

  virtual bool isFunc() const { return false; }

  std::string referenceString() const override;
  std::string textRepresentation() const override;
};

} // namespace ir
} // namespace seq
