#pragma once

#include <memory>
#include <string>
#include <vector>

#include "base.h"

namespace seq {
namespace ir {

class Var;
class Func;

class IRModule : public AttributeHolder<IRModule> {
private:
  std::vector<std::shared_ptr<Var>> globals;
  std::string name;
  std::shared_ptr<Func> baseFunc;
  std::shared_ptr<Var> argVar;

public:
  explicit IRModule(std::string name);

  std::vector<std::shared_ptr<Var>> getGlobals() { return globals; }
  void addGlobal(std::shared_ptr<Var> var);

  std::shared_ptr<Func> getBase() const { return baseFunc; }

  std::string getName() const { return name; }

  std::shared_ptr<Var> getArgVar() { return argVar; };

  std::string referenceString() const override { return "module"; };
  std::string textRepresentation() const override;
};

} // namespace ir
} // namespace seq
