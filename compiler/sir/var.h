#pragma once

#include <memory>
#include <string>
#include <vector>

#include "base.h"
#include "restypes/types.h"
#include "util/common.h"

namespace seq {
namespace ir {
class Var : public AttributeHolder {
protected:
  std::string name;
  std::shared_ptr<restypes::Type> type;

public:
  Var(std::string name, std::shared_ptr<restypes::Type> type);
  Var(std::shared_ptr<restypes::Type> type);

  void setType(std::shared_ptr<restypes::Type> type);

  std::string getName();
  std::shared_ptr<restypes::Type> getType();
};
} // namespace ir
} // namespace seq
