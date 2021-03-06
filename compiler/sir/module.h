#pragma once

#include <memory>
#include <string>
#include <vector>

#include "base.h"

namespace seq {
namespace ir {

class Var;

class IRModule : public AttributeHolder {
private:
  std::vector<std::shared_ptr<Var>> globals;
  std::string name;

public:
  IRModule(std::string name);
  IRModule(const IRModule &other);

  std::vector<std::shared_ptr<Var>> getGlobals();
  void addGlobal(std::shared_ptr<Var> var);

  std::string getName();

  std::string textRepresentation() const override;
};
} // namespace ir
} // namespace seq