#include "expr.h"
#include "func.h"

using namespace seq;
using namespace ir;

Expression::Expression(std::shared_ptr<types::Type> type) : type{type}, block{}{ }

std::shared_ptr<types::Type> seq::ir::Expression::getType() {
    return type;
}

void Expression::setBlock(std::weak_ptr<BasicBlock> block) {
    this->block = block;
}

std::weak_ptr<BasicBlock> Expression::getBlock() {
    return block;
}

std::string Expression::textRepresentation() const {
    return AttributeHolder::textRepresentation();
}

CallExpression::CallExpression(std::weak_ptr<Func> func, std::vector<std::shared_ptr<Expression>> args)
    : Expression{std::shared_ptr<Func>(func)->getRType()}{
// TODO How do I deal with exiting func type?
}

CallExpression::CallExpression(const CallExpression &other) {

}

std::string CallExpression::textRepresentation() const {
    return Expression::textRepresentation();
}

GetMemberExpression::GetMemberExpression(std::weak_ptr<Var> var, std::string field) {

}

std::string GetMemberExpression::textRepresentation() const {
    return Expression::textRepresentation();
}

PipelineExpression::PipelineExpression(const PipelineExpression &other) {

}

std::string PipelineExpression::textRepresentation() const {
    return Expression::textRepresentation();
}

MatchExpression::MatchExpression(std::shared_ptr<Expression> expr, std::shared_ptr<Pattern> pattern) {

}

std::string MatchExpression::textRepresentation() const {
    return Expression::textRepresentation();
}

IntLiteralExpression::IntLiteralExpression(seq_int_t value) {

}

std::string IntLiteralExpression::textRepresentation() const {
    return Expression::textRepresentation();
}

DoubleLiteralExpression::DoubleLiteralExpression(double value) {

}

std::string DoubleLiteralExpression::textRepresentation() const {
    return Expression::textRepresentation();
}

StringLiteralExpression::StringLiteralExpression(std::string value) {

}

std::string StringLiteralExpression::textRepresentation() const {
    return Expression::textRepresentation();
}

BoolLiteralExpression::BoolLiteralExpression(bool value) {

}

std::string BoolLiteralExpression::textRepresentation() const {
    return Expression::textRepresentation();
}

SeqLiteralExpression::SeqLiteralExpression(std::string value) {

}

std::string SeqLiteralExpression::textRepresentation() const {
    return Expression::textRepresentation();
}

VarExpression::VarExpression(std::weak_ptr<Var> var) {

}

std::string VarExpression::textRepresentation() const {
    return Expression::textRepresentation();
}

FunctionExpression::FunctionExpression(std::weak_ptr<Func> func) {

}

std::string FunctionExpression::textRepresentation() const {
    return Expression::textRepresentation();
}
