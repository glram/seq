#include <sstream>

#include "expr.h"
#include "stmt.h"
#include "var.h"

using namespace seq;
using namespace ir;

AssignStatement::AssignStatement(std::weak_ptr<Var> lhs,
                                 std::shared_ptr<Expression> rhs)
    : lhs{lhs}, rhs{rhs} {}

std::weak_ptr<Var> AssignStatement::getLhs() const { return lhs; }

std::shared_ptr<Expression> AssignStatement::getRhs() const { return rhs; }

std::string AssignStatement::textRepresentation() const {
  std::stringstream stream;

  auto lockedLhs = lhs.lock();

  stream << Statement::textRepresentation() << " ("
         << lockedLhs->textRepresentation() << ") = ("
         << rhs->textRepresentation() << ")";
  return stream.str();
}

AssignMemberStatement::AssignMemberStatement(std::weak_ptr<Var> lhs,
                                             std::shared_ptr<Expression> rhs,
                                             std::string field)
    : lhs{lhs}, rhs{rhs}, field{field} {}

std::weak_ptr<Var> AssignMemberStatement::getLhs() const { return lhs; }

std::shared_ptr<Expression> AssignMemberStatement::getRhs() const {
  return rhs;
}

std::string AssignMemberStatement::textRepresentation() const {
  std::stringstream stream;

  auto lockedLhs = lhs.lock();

  stream << Statement::textRepresentation() << " ("
         << lockedLhs->textRepresentation() << ")." << field << " = ("
         << rhs->textRepresentation() << ")";
  return stream.str();
}

ExpressionStatement::ExpressionStatement(std::shared_ptr<Expression> expr)
    : expr{expr} {}

std::string ExpressionStatement::textRepresentation() const {
  std::stringstream stream;

  stream << Statement::textRepresentation() << " ("
         << expr->textRepresentation() << ")";

  return stream.str();
}
