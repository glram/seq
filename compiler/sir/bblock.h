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
  std::vector<std::shared_ptr<Statement>> statements;
  std::shared_ptr<Terminator> terminator;

  int id;

public:
  BasicBlock(int id);
  BasicBlock(const BasicBlock &other);

  int getId() const;
  void setId(int id);

  void add(std::shared_ptr<Statement> statement);
  std::vector<std::shared_ptr<Statement>> getStatements() const;

  void setTerminator(std::shared_ptr<Terminator> terminator);
  std::shared_ptr<Terminator> getTerminator() const;

  std::string textRepresentation() const;
};
} // namespace ir
} // namespace seq