#ifndef SEQ_IR_FUNC_H
#define SEQ_IR_FUNC_H

#include <memory>
#include <vector>
#include <string>

#include "util/common.h"
#include "var.h"
#include "scope.h"
#include "module.h"
#include "base.h"

namespace seq {
    namespace ir {
        class Function : public AttributeHolder {
        private:
            std::vector<std::shared_ptr<Var>> argVars;
            std::vector<std::shared_ptr<Expression>> defaultArgValues;
            std::vector<std::string> argNames;

            std::vector<std::shared_ptr<Var>> vars;
            std::vector<std::strong_ptr<Scope>> scopes;

            std::shared_ptr<types::Type> rType;

            std::weak_ptr<IRModule> module;

        public:
            Function();
            Function(const Function &other);

            void setArgVars(std::vector<std::shared_ptr<Var>> argVars);
            std::vector<std::shared_ptr<Var>> getArgVars();

            void setDefaultArgValues(std::vector<std::shared_ptr<Expression>> defaultArgValues);
            std::vector<std::shared_ptr<Expression>> getDefaultArgValues();

            void setArgNames(std::vector<std::string> argNames);
            std::vector<std::string> getArgNames();

            std::shared_ptr<Var> getArgVar(std::string name);

            void setScopes(std::vector<std::strong_ptr<Scope>> scopes);
            std::vector<std::strong_ptr<Scope>> getScopes();

            void setRType(std::shared_ptr<types::Type> rType);
            std::shared_ptr<types::Type> getRType();

            std::weak_ptr<IRModule> getModule();

            std::string textRepresentation() const;
        };
    }
}
#endif //SEQ_IR_FUNC_H
