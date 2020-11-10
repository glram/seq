#pragma once

#include <memory>
#include <string>
#include <vector>

#include "base.h"

namespace seq {
namespace ir {

namespace common {
class SIRVisitor;
}

class Var;
class Func;

/// SIR object representing a program.
class SIRModule : public AttributeHolder<SIRModule> {
private:
  /// the global variables defined in the module
  std::vector<std::shared_ptr<Var>> globals;
  /// the module's name
  std::string name;
  /// the module's "main" function
  std::shared_ptr<Func> mainFunc;
  /// the module's argv variable
  std::shared_ptr<Var> argVar;

public:
  /// Constructs an SIR module.
  explicit SIRModule(std::string name);

  void accept(common::SIRVisitor &v);

  /// @return all globals defined in the module
  std::vector<std::shared_ptr<Var>> getGlobals() { return globals; }

  /// Adds a global variable to the module.
  /// @param var the variable.
  void addGlobal(std::shared_ptr<Var> var);

  /// @return the module's main function
  std::shared_ptr<Func> getMain() const { return mainFunc; }

  /// @return the module's name
  std::string getName() const { return name; }

  /// @return the module's argv variable
  std::shared_ptr<Var> getArgVar() { return argVar; };

  std::string referenceString() const override { return "module"; };
  std::string textRepresentation() const override;
};

} // namespace ir
} // namespace seq
