#ifndef SEQ_SOURCE_H
#define SEQ_SOURCE_H

#include <vector>
#include "expr.h"
#include "stage.h"

namespace seq {
	class Source : public Stage {
	private:
		std::vector<Expr *> sources;
		bool isSingle() const;
		types::Type *determineOutType() const;
	public:
		explicit Source(std::vector<Expr *> sources);
		void validate() override;
		void codegen(llvm::Module *module) override;
		void finalize(llvm::Module *module, llvm::ExecutionEngine *eng) override;
		static Source& make(std::vector<Expr *>);

		Source *clone(types::RefType *ref) override;
	};
}

#endif /* SEQ_SOURCE_H */
