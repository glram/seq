#ifndef SEQ_IR_BBLOCK_H
#define SEQ_IR_BBLOCK_H

#include <memory>
#include <vector>

#include "util/common.h"
#include "stmt.h"
#include "terminator.h"
#include "scope.h"
#include "base.h"

namespace seq {
    namespace ir {
        class BasicBlock : public AttributeHolder {
        private:
            std::vector<std::shared_ptr<Statement>> statements;
            std::shared_ptr<Terminator> terminator;
            std::weak_ptr<Scope> scope;

            int id;
        public:
            BasicBlock(int id);
            BasicBlock(const BasicBlock &other);

            int getId() const;
            void setId(int id);

            void add(std::shared_ptr<Statement> statement);
            std::vector<std::shared_ptr<Statement>> getStatements() const;

            void setTerminator(std::shared_ptr<Terminator> terminator);
            std::shared_ptr<Terminator> getTerminator() const;

            void setScope(std::weak_ptr<Scope> scope);
            std::weak_ptr<Scope> getScope() const;

            std::string textRepresentation() const;
        };
    }
}
#endif //SEQ_IR_BBLOCK_H
