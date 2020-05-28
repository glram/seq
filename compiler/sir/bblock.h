#pragma once

#include <memory>
#include <vector>

#include "base.h"

namespace seq {
namespace ir {

class Statement;
class Terminator;

class BasicBlock : public AttributeHolder {
private:
  static int currentId;

  std::vector<std::shared_ptr<Statement>> statements;
  std::shared_ptr<Terminator> terminator;

  int id;

public:
  BasicBlock();
  BasicBlock(const BasicBlock &other);

  void add(std::shared_ptr<Statement> statement);
  std::vector<std::shared_ptr<Statement>> getStatements() const;

  void setTerminator(std::shared_ptr<Terminator> terminator);
  std::shared_ptr<Terminator> getTerminator() const;

  int getId();

  virtual std::string referenceString() const override;
  std::string textRepresentation() const override;
};
} // namespace ir
} // namespace seq