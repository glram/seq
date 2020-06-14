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

public:
  IRModule(std::string name);
  IRModule(const IRModule &other);

  std::vector<std::shared_ptr<Var>> getGlobals();
  void addGlobal(std::shared_ptr<Var> var);

  std::shared_ptr<Func> getBase() const { return baseFunc; }

  std::string getName();

  std::string referenceString() const override { return "module"; };
  std::string textRepresentation() const override;
};
} // namespace ir
} // namespace seq