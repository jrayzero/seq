#include "seq/seq.h"
#include "seq/stage.h"
#include "seq/basestage.h"
#include "seq/mem.h"
#include "seq/exc.h"
#include "seq/var.h"

using namespace seq;
using namespace llvm;

/*
 * In the standalone case, pipelines are responsible for
 * getting added to whatever base module/function they are
 * a part of, not variables. For example:
 *
 *   var a = ...
 *   var b = a | <*>
 *
 * The * pipeline will be added by "a |" not by "b =". This
 * makes everything easier and more consistent, but isn't easy
 * to do via the C++-embedded DSL, since we can have things
 * like this:
 *
 *   var x = m[i]
 *
 * There's nothing to incorporate the memory access besides
 * the var assignment. In the standalone language we can always
 * keep track of the context we're in and add it automatically.
 */

Var::Var(bool standalone) : stage(nullptr), standalone(standalone)
{
}

Var::Var(Pipeline pipeline, bool standalone) : Var(standalone)
{
	*this = pipeline;
}

types::Type *Var::getType(Stage *caller) const
{
	if (!isAssigned())
		throw exc::SeqException("variable used before assigned");

	return stage->getOutType();
}

ValMap Var::outs(Stage *caller) const
{
	if (!isAssigned())
		throw exc::SeqException("variable used before assigned");

	return stage->outs;
}

Stage *Var::getStage() const
{
	if (!isAssigned())
		throw exc::SeqException("variable used before assigned");

	return stage;
}

bool Var::isAssigned() const
{
	return stage != nullptr;
}

BaseFunc *Var::getBase() const
{
	if (!isAssigned())
		throw exc::SeqException("variable used before assigned");

	return stage->getBase();
}

Pipeline Var::operator|(Pipeline to)
{
	if (!isAssigned())
		throw exc::SeqException("variable used before assigned");

	if (to.isAdded())
		throw exc::MultiLinkException(*to.getHead());

	ensureConsistentBase(to.getHead()->getBase());
	BaseFunc *base = getBase();
	to.getHead()->setBase(base);
	types::Type *inType = standalone ? (types::Type *)types::AnyType::get() : (types::Type *)types::VoidType::get();
	BaseStage& begin = BaseStage::make(inType, getType(stage), stage);
	begin.setBase(base);
	begin.outs = outs(stage);

	if (stage)
		stage->addWeakNext(to.getHead());

	Pipeline full = begin | to;

	if (!standalone)
		base->add(full);

	return full;
}

Pipeline Var::operator&(PipelineList& to)
{
	Pipeline first, last;

	for (auto *n = to.head; n; n = n->next) {
		if (n->isVar)
			throw exc::SeqException("cannot send var output to another var");
		else {
			last = *this | n->p;

			if (n == to.head)
				first = last;
		}
	}

	return {first.getHead(), last.getTail()};
}

Pipeline Var::operator||(Pipeline to)
{
	if (!isAssigned())
		throw exc::SeqException("variable used before assigned");

	if (to.isAdded())
		throw exc::MultiLinkException(*to.getHead());

	ensureConsistentBase(to.getHead()->getBase());
	BaseFunc *base = getBase();
	to.getHead()->setBase(base);
	BaseStage& begin = BaseStage::make(types::AnyType::get(), getType(stage), stage);
	begin.setBase(base);
	begin.outs = outs(stage);

	if (stage)
		stage->addWeakNext(to.getHead());

	Pipeline full = begin | to;
	return full;
}

Pipeline Var::operator&&(PipelineList& to)
{
	Pipeline last;

	for (auto *n = to.head; n; n = n->next) {
		if (n->isVar)
			throw exc::SeqException("cannot send var output to another var");
		else {
			Pipeline p = *this || n->p;

			if (n == to.head)
				last = p;
			else
				last = last | p;
		}
	}

	return last;
}

void Var::assign(Pipeline to)
{
	if (isAssigned())
		throw exc::SeqException("variable cannot be assigned twice");

	stage = to.getTail();
}

Var& Var::operator=(Pipeline to)
{
	assign(to);

	if (!standalone) {
		BaseFunc *base = getBase();

		if (!to.isAdded()) {
			BaseStage &begin = BaseStage::make(types::VoidType::get(), types::VoidType::get());
			begin.setBase(base);
			Pipeline full = begin | to;
			base->add(full);
		}
	}

	return *this;
}

LoadStore& Var::operator[](Var& idx)
{
	ensureConsistentBase(idx.getBase());
	return LoadStore::make(this, &idx);
}

LoadStore& Var::operator[](seq_int_t idx)
{
	return LoadStore::make(this, idx);
}

void Var::ensureConsistentBase(BaseFunc *base)
{
	if (stage && stage->getBase() && base && stage->getBase() != base)
		throw exc::SeqException("cannot use variable in different context than where it was assigned");
}

Const::Const(types::Type *type) :
    Var(), type(type), outsMap(new std::map<SeqData, Value *>)
{
}

types::Type *Const::getType(Stage *caller) const
{
	return type;
}

ValMap Const::outs(Stage *caller) const
{
	return outsMap;
}

Stage *Const::getStage() const
{
	return nullptr;
}

bool Const::isAssigned() const
{
	return true;
}

BaseFunc *Const::getBase() const
{
	return nullptr;
}

ConstInt::ConstInt(seq_int_t n) : Const(types::IntType::get()), n(n)
{
}

ValMap ConstInt::outs(Stage *caller) const
{
	if (caller && outsMap->empty()) {
		LLVMContext& context = caller->getBase()->getContext();
		BasicBlock *preambleBlock = caller->getBase()->getPreamble();
		Value *var = makeAlloca(ConstantInt::get(seqIntLLVM(context), (uint64_t)n, true), preambleBlock);
		outsMap->insert({SeqData::INT, var});
	}

	return Const::outs(caller);
}

ConstInt& ConstInt::get(seq_int_t n)
{
	return *new ConstInt(n);
}

ConstFloat::ConstFloat(double f) : Const(types::FloatType::get()), f(f)
{
}

ValMap ConstFloat::outs(Stage *caller) const
{
	if (caller && outsMap->empty()) {
		LLVMContext& context = caller->getBase()->getContext();
		BasicBlock *preambleBlock = caller->getBase()->getPreamble();
		Value *var = makeAlloca(ConstantFP::get(Type::getDoubleTy(context), f), preambleBlock);
		outsMap->insert({SeqData::FLOAT, var});
	}

	return Const::outs(caller);
}

ConstFloat& ConstFloat::get(double f)
{
	return *new ConstFloat(f);
}

Latest::Latest() : Var()
{
}

static void validateCaller(Stage *caller)
{
	if (!caller)
		throw exc::SeqException("misplaced _");

	if (!caller->getPrev())
		throw exc::StageException("stage has no predecessor", *caller);
}

types::Type *Latest::getType(Stage *caller) const
{
	validateCaller(caller);
	return caller->getPrev()->getOutType();
}

ValMap Latest::outs(Stage *caller) const
{
	validateCaller(caller);
	return caller->getPrev()->outs;
}

Stage *Latest::getStage() const
{
	return nullptr;
}

bool Latest::isAssigned() const
{
	return true;
}

BaseFunc *Latest::getBase() const
{
	return nullptr;
}

Latest& Latest::get()
{
	static auto *latest = new Latest();
	return *latest;
}
