#pragma once

#include <memory>
#include <string>
#include <vector>

#include "base.h"

namespace seq {
namespace ir {

class Expression;
class Var;

class Statement : public AttributeHolder {};

class AssignStatement : public Statement {
private:
  std::weak_ptr<Var> lhs;
  std::shared_ptr<Expression> rhs;

public:
  AssignStatement(std::weak_ptr<Var> lhs, std::shared_ptr<Expression> rhs);

  std::weak_ptr<Var> getLhs() const;
  std::shared_ptr<Expression> getRhs() const;

  std::string textRepresentation() const;
};

class AssignMemberStatement : public Statement {
private:
  std::weak_ptr<Var> lhs;
  std::shared_ptr<Expression> rhs;
  std::string field;

public:
  AssignMemberStatement(std::weak_ptr<Var> lhs, std::shared_ptr<Expression> rhs,
                        std::string field);

  std::weak_ptr<Var> getLhs() const;
  std::shared_ptr<Expression> getRhs() const;

  std::string textRepresentation() const;
};

class ExpressionStatement : public Statement {
private:
  std::shared_ptr<Expression> expr;

public:
  ExpressionStatement(std::shared_ptr<Expression> expr);

  std::string textRepresentation() const;
};
} // namespace ir
} // namespace seq
