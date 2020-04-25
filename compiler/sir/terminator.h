#ifndef SEQ_IR_TERMINATOR_H
#define SEQ_IR_TERMINATOR_H

#include "util/common.h"
#include "bblock.h"
#include "expr.h"

namespace seq {
    namespace ir {
        class Terminator : public SrcObject {

        };

        class ForTerminator : public Terminator {
        private:
            std::weak_ptr<BasicBlock> setup;
            std::weak_ptr<BasicBlock> update;
            std::weak_ptr<BasicBlock> body;
            std::shared_ptr<Expression> cond;
        };

        class EndForSetupTerminator : public Terminator {
        private:
            std::weak_ptr<BasicBlock> forBlock;
        };

        class ContinueForTerminator : public Terminator {
        private:
            std::weak_ptr<BasicBlock> forBlock;
        };

        class JumpTerminator : public Terminator {
        private:
            std::weak_ptr<BasicBlock> dst;
        };

        class CondJumpTerminator : public Terminator {
        private:
            std::weak_ptr<BasicBlock> tDst;
            std::weak_ptr<BasicBlock> fDst;
            std::shared_ptr<Expression> cond;
        };

        class ReturnTerminator : public Terminator {

        };

        class YieldTerminator : public Terminator {
        private:
            std::weak_ptr<BasicBlock> dst;

            // TODO - reverse yield
            std::weak_ptr<Var> result
        };

        class ThrowTerminator : public Terminator {
        private:
            std::shared_ptr<Expression> expr;
        };
    }
}
#endif //SEQ_IR_TERMINATOR_H
