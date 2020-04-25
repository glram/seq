#ifndef SEQ_IR_FUNC_H
#define SEQ_IR_FUNC_H

#include "util/common.h"
#include "var.h"
#include "scope.h"

namespace seq {
    namespace ir {
        class Function : public SrcObject {
        private:
            std::vector<std::shared_ptr<Var>> argVars;
            std::vector<std::shared_ptr<Var>> vars;
            std::vector<std::shared_ptr<Scope>> scopes;
            std::string name;
        };
    }
}
#endif //SEQ_IR_FUNC_H
