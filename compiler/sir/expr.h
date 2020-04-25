#ifndef SEQ_IR_EXPR_H
#define SEQ_IR_EXPR_H

#include <compiler/lang/func.h>
#include "util/common.h"

namespace seq {
    namespace ir {
        class Expression : public SrcObject {

        };

        class CallExpression : public Expression {
        private:
            std::weak_ptr<Func> func;
            std::vector<std::shared_ptr<Expression>> args;
        };

        class GetMemberExpression : public Expression {
        private:
            std::weak_ptr<Var> expr;
            std::string field;

        };

        class PipelineExpression : public Expression {
        private:
            std::vector<std::shared_ptr<Expression>> args;
            std::vector<bool> parallel;
        };

        class MatchExpression : public Expression {
        private:
            std::shared_ptr<Expression> expr;
            // TODO: Patterns
        };

        class LiteralExpression : public Expression {

        };

        class IntLiteralExpression : public LiteralExpression {
        private:
            int value;
        };

        class FloatLiteralExpression : public LiteralExpression {
        private:
            float value;

        };

        class StringLiteralExpression : public LiteralExpression {
        private:
            std::string value;
        };

        class BoolLiteralExpression : public LiteralExpression {
        private:
            bool value;

        };

        class VarExpression : public Expression {
        private:
            std::weak_ptr<Var> var;
        };

        class FunctionExpression : public Expression {
            std::weak_ptr<Func> func;
        };
    }
}
#endif //SEQ_IR_EXPR_H
