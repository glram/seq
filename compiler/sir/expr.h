#ifndef SEQ_IR_EXPR_H
#define SEQ_IR_EXPR_H

#include <memory>
#include <vector>
#include <string>

#include "util/common.h"
#include "types/types.h"
#include "bblock.h"
#include "pattern.h"
#include "base.h"
#include "func.h"

namespace seq {
    namespace ir {
        class Expression : public AttributeHolder {
        private:
            std::shared_ptr<types::Type> type;
            std::weak_ptr<BasicBlock> block;

        public:
            explicit Expression(std::shared_ptr<types::Type> type);

            std::shared_ptr<types::Type> getType();

            void setBlock(std::weak_ptr<BasicBlock> block);
            std::weak_ptr<BasicBlock> getBlock();

            virtual std::string textRepresentation() const;
        };

        class CallExpression : public Expression {
        private:
            std::weak_ptr<Func> func;
            std::vector<std::shared_ptr<Expression>> args;

        public:
            explicit CallExpression(std::weak_ptr<Func> func, std::vector<std::shared_ptr<Expression>> args);
            CallExpression(const CallExpression &other);

            std::string textRepresentation() const;
        };

        class GetMemberExpression : public Expression {
        private:
            std::weak_ptr<Var> var;
            std::string field;

        public:
            explicit GetMemberExpression(std::weak_ptr<Var> var, std::string field);

            std::string textRepresentation() const;
        };

        class PipelineExpression : public Expression {
        private:
            std::vector<std::shared_ptr<Expression>> stages;
            std::vector<bool> parallel;

        public:
            explicit PiplineExpression(std::vector<std::shared_ptr<Expression>> stages, std::vector<bool> parallel);
            PipelineExpression(const PipelineExpression &other);

            std::string textRepresentation() const;
        };

        class MatchExpression : public Expression {
        private:
            std::shared_ptr<Expression> expr;
            std::shared_ptr<Pattern> pattern;

        public:
            explicit MatchExpression(std::shared_ptr<Expression> expr, std::shared_ptr<Pattern> pattern);

            std::string textRepresentation() const;
        };

        class LiteralExpression : public Expression {

        };

        class IntLiteralExpression : public LiteralExpression {
        private:
            seq_int_t value;

        public:
            explicit IntLiteralExpression(seq_int_t value);

            std::string textRepresentation() const;
        };

        class DoubleLiteralExpression : public LiteralExpression {
        private:
            double value;

        public:
            explicit DoubleLiteralExpression(double value);

            std::string textRepresentation() const;
        };

        class StringLiteralExpression : public LiteralExpression {
        private:
            std::string value;

        public:
            explicit StringLiteralExpression(std::string value);

            std::string textRepresentation() const;
        };

        class BoolLiteralExpression : public LiteralExpression {
        private:
            bool value;

        public:
            explicit BoolLiteralExpression(bool value);

            std::string textRepresentation() const;
        };

        class SeqLiteralExpression : public LiteralExpression {
        private:
            std::string value;

        public:
            explicit SeqLiteralExpression(std::string value);

            std::string textRepresentation() const;
        };

        class VarExpression : public Expression {
        private:
            std::weak_ptr<Var> var;

        public:
            explicit VarExpression(std::weak_ptr<Var> var);

            std::string textRepresentation() const;
        };

        class FunctionExpression : public Expression {
            std::weak_ptr<Func> func;

        public:
            explicit FunctionExpression(std::weak_ptr<Func> func);

            std::string textRepresentation() const;
        };
    }
}
#endif //SEQ_IR_EXPR_H
