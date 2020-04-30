#ifndef SEQ_IR_MODULE_H
#define SEQ_IR_MODULE_H

#include <memory>
#include <vector>
#include <string>

#include "util/common.h"
#include "var.h"
#include "func.h"
#include "base.h"

namespace seq {
    namespace ir {
        class IRModule : public AttributeHolder {
        private:
            std::vector<std::shared_ptr<Var>> globalVariables;
            std::vector<std::shared_ptr<Function>> functions;

        public:
            IRModule();
            IRModule(const IRModule &other);

            std::vector<std::shared_ptr<Var>> getGlobalVariables();
            void addGlobalVariable(std::shared_ptr<Var> var);

            std::vector<std::shared_ptr<Function>> getFunctions();
            void addFunction(std::shared_ptr<Function> function);

            std::string getName();

            std::string textRepresentation() const;
        };
    }
}

#endif //SEQ_IR_MODULE_H
