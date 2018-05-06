#ifndef SEQ_LOOKUPEXPR_H
#define SEQ_LOOKUPEXPR_H

#include "expr.h"

namespace seq {
	class ArrayLookupExpr : public Expr {
	private:
		Expr *arr;
		Expr *idx;
	public:
		ArrayLookupExpr(Expr *arr, Expr *idx);
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock *block) override;
	};
}

#endif /* SEQ_LOOKUPEXPR_H */
