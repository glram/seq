#pragma once

#include <memory>
#include <string>
#include <vector>

#include "base.h"
#include "types/types.h"
#include "util/common.h"

namespace seq {
namespace ir {
class Var : public AttributeHolder<Var> {
private:
  static int varNum;

  // TODO: refactor out
protected:
  std::string name;
  std::shared_ptr<types::Type> type;
  int id;

public:
  Var(std::string name, std::shared_ptr<types::Type> type);
  Var(std::shared_ptr<types::Type> type);

  void setType(std::shared_ptr<types::Type> type);

  std::string getName();
  std::shared_ptr<types::Type> getType();
  int getId();

  std::string referenceString() const override;
  std::string textRepresentation() const override;
};
} // namespace ir
} // namespace seq
