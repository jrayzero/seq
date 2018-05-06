#include "seq/seq.h"
#include "seq/numexpr.h"

using namespace seq;
using namespace llvm;

IntExpr::IntExpr(seq_int_t n) : Expr(types::IntType::get()), n(n)
{
}

Value *IntExpr::codegen(BaseFunc *base, BasicBlock *block)
{
	LLVMContext& context = block->getContext();
	return ConstantInt::get(seqIntLLVM(context), (uint64_t)n, true);
}
