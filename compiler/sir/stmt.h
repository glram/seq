#ifndef SEQ_IR_STMT_H
#define SEQ_IR_STMT_H

#include "util/common.h"
#include "var.h"
#include "expr.h"

namespace seq {
    namespace ir {
        class Statement : public SrcObject {

        };

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
    }
}
#endif //SEQ_IR_STMT_H
