#pragma once

#include <memory>
#include <string>
#include <vector>

#include "base.h"
#include "restypes/types.h"
#include "util/common.h"

namespace seq {
namespace ir {

class BasicBlock;
class Var;
class Func;
class Pattern;

class Expression : public AttributeHolder {
private:
  std::shared_ptr<restypes::Type> type;
  std::weak_ptr<BasicBlock> block;

public:
  Expression();
  explicit Expression(std::shared_ptr<restypes::Type> type);

  std::shared_ptr<restypes::Type> getType() const;
  void setType(std::shared_ptr<restypes::Type> type);

  void setBlock(std::weak_ptr<BasicBlock> block);
  std::weak_ptr<BasicBlock> getBlock();

  virtual std::string textRepresentation() const override;
};

class VarExpression : public Expression {
private:
  std::weak_ptr<Var> var;

public:
  explicit VarExpression(std::weak_ptr<Var> var);

  std::weak_ptr<Var> getVar();

  std::string textRepresentation() const override;
};

class CallExpression : public Expression {
private:
  std::shared_ptr<Expression> func;
  std::vector<std::shared_ptr<Expression>> args;

public:
  explicit CallExpression(std::shared_ptr<Expression> func,
                          std::vector<std::shared_ptr<Expression>> args);
  CallExpression(const CallExpression &other);

  std::string textRepresentation() const override;
};

class GetMemberExpression : public Expression {
private:
  std::shared_ptr<Expression> lhs;
  std::string field;

public:
  explicit GetMemberExpression(std::shared_ptr<Expression> lhs,
                               std::string field);

  std::string textRepresentation() const override;
};

class PipelineExpression : public Expression {
private:
  std::vector<std::shared_ptr<Expression>> stages;
  std::vector<bool> parallel;

public:
  explicit PipelineExpression(std::vector<std::shared_ptr<Expression>> stages,
                              std::vector<bool> parallel);
  PipelineExpression(const PipelineExpression &other);

  std::string textRepresentation() const override;
};

class MatchExpression : public Expression {
private:
  std::shared_ptr<Expression> expr;
  std::shared_ptr<Pattern> pattern;

public:
  explicit MatchExpression(std::shared_ptr<Expression> expr,
                           std::shared_ptr<Pattern> pattern);

  std::string textRepresentation() const override;
};

class LiteralExpression : public Expression {
public:
  explicit LiteralExpression(std::shared_ptr<restypes::Type> type);
};

class IntLiteralExpression : public LiteralExpression {
private:
  seq_int_t value;

public:
  explicit IntLiteralExpression(seq_int_t value);

  std::string textRepresentation() const override;
};

class DoubleLiteralExpression : public LiteralExpression {
private:
  double value;

public:
  explicit DoubleLiteralExpression(double value);

  std::string textRepresentation() const override;
};

class StringLiteralExpression : public LiteralExpression {
private:
  std::string value;

public:
  explicit StringLiteralExpression(std::string value);

  std::string textRepresentation() const override;
};

class BoolLiteralExpression : public LiteralExpression {
private:
  bool value;

public:
  explicit BoolLiteralExpression(bool value);

  std::string textRepresentation() const override;
};

class SeqLiteralExpression : public LiteralExpression {
private:
  std::string value;

public:
  explicit SeqLiteralExpression(std::string value);

  std::string textRepresentation() const override;
};
} // namespace ir
} // namespace seq