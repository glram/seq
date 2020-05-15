#include <algorithm>
#include <iterator>
#include <sstream>

#include "expr.h"
#include "pattern.h"
#include "var.h"

using namespace seq;
using namespace ir;

Expression::Expression() : type{nullptr} {}

Expression::Expression(std::shared_ptr<restypes::Type> type) : type{type} {}

std::shared_ptr<restypes::Type> Expression::getType() const { return type; }

void Expression::setType(std::shared_ptr<restypes::Type> type) {
  this->type = type;
}

void Expression::setBlock(std::weak_ptr<BasicBlock> block) {
  this->block = block;
}

std::weak_ptr<BasicBlock> Expression::getBlock() { return block; }

std::string Expression::textRepresentation() const {
  return AttributeHolder::textRepresentation();
}

VarExpression::VarExpression(std::weak_ptr<Var> var)
    : Expression{var.lock()->getType()}, var{var} {}

std::string VarExpression::textRepresentation() const {
  std::stringstream stream;

  stream << Expression::textRepresentation() << " $" << var.lock()->getName();
  return stream.str();
}

CallExpression::CallExpression(std::shared_ptr<Expression> func,
                               std::vector<std::shared_ptr<Expression>> args)
    : Expression{}, func{func}, args{args} {

  // TODO functors
  auto funcType =
      std::dynamic_pointer_cast<restypes::FuncType>(func->getType());
  if (funcType)
    setType(funcType->getRType());
}

CallExpression::CallExpression(const CallExpression &other)
    : Expression{other.getType()}, func{other.func}, args{} {
  std::copy(other.args.begin(), other.args.end(), std::back_inserter(args));
}

std::string CallExpression::textRepresentation() const {
  std::stringstream stream;

  stream << Expression::textRepresentation() << " ("
         << func->textRepresentation() << ")()";
  return stream.str();
}

GetMemberExpression::GetMemberExpression(std::shared_ptr<Expression> lhs,
                                         std::string field)
    : Expression{lhs->getType()->getMemberType(field)}, lhs{lhs}, field{field} {
}

std::string GetMemberExpression::textRepresentation() const {
  std::stringstream stream;

  stream << Expression::textRepresentation() << " ("
         << lhs->textRepresentation() << ")." << field;
  return stream.str();
}

PipelineExpression::PipelineExpression(
    std::vector<std::shared_ptr<Expression>> stages, std::vector<bool> parallel)
    : Expression{}, stages{stages}, parallel{parallel} {

  auto type = (!stages.empty()) ? std::dynamic_pointer_cast<restypes::FuncType>(
                                      stages[stages.size() - 1]->getType())
                                : std::shared_ptr<restypes::FuncType>();
  if (type)
    setType(type->getRType());
}

PipelineExpression::PipelineExpression(const PipelineExpression &other)
    : Expression{other.getType()}, parallel{parallel}, stages{} {
  std::copy(other.stages.begin(), other.stages.end(),
            std::back_inserter(stages));
}

std::string PipelineExpression::textRepresentation() const {
  std::stringstream stream;
  stream << Expression::textRepresentation();
  for (int i = 0; i < stages.size(); i++) {
    stream << "(" << stages[i]->textRepresentation() << ")";
    if (i < parallel.size()) {
      stream << ((parallel[i]) ? "||>" : "|>");
    }
  }
  return stream.str();
}

MatchExpression::MatchExpression(std::shared_ptr<Expression> expr,
                                 std::shared_ptr<Pattern> pattern)
    : Expression{std::static_pointer_cast<restypes::Type>(restypes::kBoolType)},
      expr{expr}, pattern{pattern} {}

std::string MatchExpression::textRepresentation() const {
  std::stringstream stream;

  stream << Expression::textRepresentation() << " ("
         << expr->textRepresentation() << ") match ("
         << pattern->textRepresentation() << ")";
  return stream.str();
}

LiteralExpression::LiteralExpression(std::shared_ptr<restypes::Type> type)
    : Expression{type} {}

IntLiteralExpression::IntLiteralExpression(seq_int_t value)
    : LiteralExpression{std::static_pointer_cast<restypes::Type>(
          restypes::kIntType)},
      value{value} {}

std::string IntLiteralExpression::textRepresentation() const {
  std::stringstream stream;
  stream << Expression::textRepresentation() << " " << value;
  return stream.str();
}

DoubleLiteralExpression::DoubleLiteralExpression(double value)
    : LiteralExpression{std::static_pointer_cast<restypes::Type>(
          restypes::kDoubleType)},
      value{value} {}

std::string DoubleLiteralExpression::textRepresentation() const {
  std::stringstream stream;
  stream << Expression::textRepresentation() << " " << value;
  return stream.str();
}

StringLiteralExpression::StringLiteralExpression(std::string value)
    : LiteralExpression{std::static_pointer_cast<restypes::Type>(
          restypes::kStringType)},
      value{value} {}

std::string StringLiteralExpression::textRepresentation() const {
  std::stringstream stream;
  stream << Expression::textRepresentation() << " '" << value << "'";
  return stream.str();
}

BoolLiteralExpression::BoolLiteralExpression(bool value)
    : LiteralExpression{std::static_pointer_cast<restypes::Type>(
          restypes::kBoolType)},
      value{value} {}

std::string BoolLiteralExpression::textRepresentation() const {
  std::stringstream stream;
  stream << Expression::textRepresentation() << " "
         << ((value) ? "True" : "False");
  return stream.str();
}

SeqLiteralExpression::SeqLiteralExpression(std::string value)
    : LiteralExpression{std::static_pointer_cast<restypes::Type>(
          restypes::kSeqType)},
      value{value} {}

std::string SeqLiteralExpression::textRepresentation() const {
  std::stringstream stream;
  stream << Expression::textRepresentation() << " s'" << value << "'";
  return stream.str();
}
