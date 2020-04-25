#ifndef SEQ_IR_VAR_H
#define SEQ_IR_VAR_H

#include "util/common.h"

namespace seq {
    namespace ir {
        class Var : public SrcObject {
        private:
            std::string name;
            // TODO: Type info...

        };
    }
}
#endif //SEQ_IR_VAR_H
