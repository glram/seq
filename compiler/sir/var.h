#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base.h"
#include "types/types.h"
#include "util/common.h"

namespace seq {
namespace ir {

namespace common {
class SIRVisitor;
}

class SIRModule;

/// SIR object representing a variable.
class Var : public AttributeHolder<Var> {
private:
  /// globally shared variable counter (variables have unique ids).
  static int currentId;

protected:
  /// the name
  std::string name;
  /// the variable's type
  std::shared_ptr<types::Type> type;

  /// true if the variable is global, false otherwise
  bool global;
  /// the variable's id
  int id;

public:
  /// Constructs a variable.
  /// @param name the variable's name
  /// @param type the variable's type
  /// @param global true if global, false otherwise
  Var(std::string name, std::shared_ptr<types::Type> type, bool global = false)
      : name(std::move(name)), type(std::move(type)), global(global), id(currentId++){};

  /// Constructs an unnamed variable.
  /// @param type the variable's type
  explicit Var(std::shared_ptr<types::Type> type) : Var("", std::move(type)) {}

  virtual ~Var() = default;

  virtual void accept(common::SIRVisitor &v);

  /// Resets the globally shared variable counter. Should only be used in testing.
  static void resetId();

  /// Sets the variable's type.
  /// @param t the new type
  virtual void setType(std::shared_ptr<types::Type> t) { type = std::move(t); }

  /// Sets the variable's name.
  /// @param n the new name
  void setName(std::string n) { name = std::move(n); }

  /// @return the variable's name
  std::string getName() const { return name; }

  /// @tparam the expected type of SIR "type"
  /// @return the type of the variable
  template <typename Type = types::Type> std::shared_ptr<Type> getType() {
    return std::static_pointer_cast<Type>(type);
  }

  /// @return the variable's id
  int getId() const { return id; }

  /// Sets the variable as global.
  void setGlobal() { global = true; }

  /// @return true if global, false otherwise
  bool isGlobal() const { return global; }

  /// @return true if the variable is a function, false otherwise
  virtual bool isFunc() const { return false; }

  std::string referenceString() const override;
  std::string textRepresentation() const override;
};

} // namespace ir
} // namespace seq
