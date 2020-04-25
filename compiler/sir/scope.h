#ifndef SEQ_IR_SCOPE_H
#define SEQ_IR_SCOPE_H

#include "util/common.h"
#include "bblock.h"
#include "var.h"

namespace seq {
    namespace ir {
        class Scope : public SrcObject {

        };

        class TryCatchScope : public Scope {
        private:
            std::shared_ptr<TryCatchScope> tryScope;
            std::shared_ptr<TryCatchScope> finallyScope;
            std::vector<std::shared_ptr<TryCatchScope>> catchScopes;
            std::vector<std::shared_ptr<Var>> catchVars;
        };

        class BlockScope : public Scope {
        private:
            std::shared_ptr<BasicBlock> block;
        };
    }
}
#endif //SEQ_IR_SCOPE_H
