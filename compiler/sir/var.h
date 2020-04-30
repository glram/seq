#ifndef SEQ_IR_VAR_H
#define SEQ_IR_VAR_H

#include <memory>
#include <vector>
#include <string>

#include "util/common.h"
#include "base.h"

namespace seq {
    namespace ir {
        class Var : public AttributeHolder {
        private:
            std::string name;
            // TODO: Type info...

        };
    }
}
#endif //SEQ_IR_VAR_H
