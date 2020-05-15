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
  std::unique_ptr<Expression> rhs;
};

class AssignMemberStatement : public Statement {
private:
  std::weak_ptr<Var> lhs;
  std::unique_ptr<Expression> rhs;
  std::string field;
};

class ExpressionStatement : public Statement {
private:
  std::unique_ptr<Expression> expr;
};
} // namespace ir
} // namespace seq
