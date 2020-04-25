#ifndef SEQ_IR_MODULE_H
#define SEQ_IR_MODULE_H

#include "util/common.h"
#include "var.h"
#include "func.h"

namespace seq {
    namespace ir {
        class IRModule : public SrcObject {
        private:
            std::vector<std::shared_ptr<Var>> globalVariables;
            std::vector<std::shared_ptr<Function>> functions;
            std::string name;
        };
    }
}

#endif //SEQ_IR_MODULE_H
