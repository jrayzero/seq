#ifndef SEQ_GETELEMEXPR_H
#define SEQ_GETELEMEXPR_H

#include "expr.h"

namespace seq {

	class GetElemExpr : public Expr {
	private:
		Expr *rec;
		std::string memb;
	public:
		GetElemExpr(Expr *rec, std::string memb);
		GetElemExpr(Expr *rec, seq_int_t idx);
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType() const override;
		GetElemExpr *clone(Generic *ref) override;
	};

	class GetStaticElemExpr : public Expr {
	private:
		types::Type *type;
		std::string memb;
	public:
		GetStaticElemExpr(types::Type *type, std::string memb);
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::Type *getType() const override;
		GetStaticElemExpr *clone(Generic *ref) override;
	};

	class MethodExpr : public Expr {
	private:
		Expr *expr;
		std::string name;
		std::vector<types::Type *> types;
	public:
		MethodExpr(Expr *expr, std::string method, std::vector<types::Type *> realizedTypes);
		llvm::Value *codegen(BaseFunc *base, llvm::BasicBlock*& block) override;
		types::MethodType *getType() const override;
		MethodExpr *clone(Generic *ref) override;
	};

}

#endif /* SEQ_GETELEMEXPR_H */
