#pragma once

#include <memory>
#include <string>
#include <vector>

#include "base.h"
#include "util/common.h"
#include "restypes/types.h"

namespace seq {
namespace ir {
class Var : public AttributeHolder {
private:
  std::string name;
  std::shared_ptr<restypes::Type> type;

public:
  Var(std::string name, std::shared_ptr<restypes::Type> type);

  std::string getName();
  std::shared_ptr<restypes::Type> getType();
};
} // namespace ir
} // namespace seq
