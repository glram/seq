#ifndef SEQ_IR_BBLOCK_H
#define SEQ_IR_BBLOCK_H

#include "util/common.h"
#include "stmt.h"
#include "terminator.h"

namespace seq {
    namespace ir {
        class BasicBlock : public SrcObject {
        private:
            std::vector<std::shared_ptr<Statement>> statements;
            std::shared_ptr<Terminator> terminator;
            std::string name;
        };
    }
}
#endif //SEQ_IR_BBLOCK_H
