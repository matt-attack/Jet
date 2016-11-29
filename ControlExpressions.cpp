#include "Expressions.h"
#include "Compiler.h"
#include "Parser.h"
#include "Types/Function.h"
#include "ControlExpressions.h"

using namespace Jet;

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DerivedTypes.h>


CValue DefaultExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&token);

	SwitchExpression* sw = dynamic_cast<SwitchExpression*>(this->parent->parent);
	if (sw == 0)
		context->root->Error("Cannot use default expression outside of a switch!", token);

	//create a new block for this case
	llvm::BasicBlock* block = sw->def;

	//add the case to the switch
	bool is_first = sw->first_case;
	sw->first_case = false;

	//jump to end block
	if (!is_first)
		context->root->builder.CreateBr(sw->switch_end);

	//start using our new block
	context->function->f->getBasicBlockList().push_back(block);
	context->root->builder.SetInsertPoint(block);
	return CValue();
}

CValue CaseExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&token);

	SwitchExpression* sw = dynamic_cast<SwitchExpression*>(this->parent->parent);
	if (sw == 0)
		context->root->Error("Cannot use case expression outside of a switch!", token);

	//create a new block for this case
	llvm::BasicBlock* block = llvm::BasicBlock::Create(llvm::getGlobalContext(), "case" + value.text);

	//add the case to the switch
	bool is_first = sw->AddCase(context->root->builder.getInt32(std::stol(this->value.text)), block);

	//jump to end block
	if (!is_first)
		context->root->builder.CreateBr(sw->switch_end);

	//start using our new block
	context->function->f->getBasicBlockList().push_back(block);
	context->root->builder.SetInsertPoint(block);
	return CValue();
}

CValue SwitchExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&token);

	CValue value = this->var->Compile(context);
	if (value.type->type != Types::Int)
		context->root->Error("Argument to Case Statement Must Be an Integer", token);

	this->switch_end = llvm::BasicBlock::Create(llvm::getGlobalContext(), "switchend");

	//look for all case and default expressions
	std::vector < CaseExpression* > cases;
	for (auto expr : this->block->statements)
	{
		auto Case = dynamic_cast<CaseExpression*>(expr);
		if (Case)
			cases.push_back(Case);

		//add default parser and expression
		else if (auto def = dynamic_cast<DefaultExpression*>(expr))
		{
			//do default
			if (this->def)
				context->root->Error("Multiple defaults defined for the same switch!", token);
			this->def = llvm::BasicBlock::Create(llvm::getGlobalContext(), "switchdefault");
		}
	}

	bool no_def = def ? false : true;
	if (def == 0)
	{
		//create default block at end if there isnt one
		this->def = llvm::BasicBlock::Create(llvm::getGlobalContext(), "switchdefault");
	}

	//create the switch instruction
	this->sw = context->root->builder.CreateSwitch(value.val, def, cases.size());

	//compile the block
	this->block->Compile(context);

	context->root->builder.CreateBr(this->switch_end);

	if (no_def)
	{
		//insert and create a dummy default
		context->function->f->getBasicBlockList().push_back(def);
		context->root->builder.SetInsertPoint(def);
		context->root->builder.CreateBr(this->switch_end);
	}

	//start using end
	context->function->f->getBasicBlockList().push_back(this->switch_end);
	context->root->builder.SetInsertPoint(this->switch_end);

	return CValue();
}

Type* CallExpression::TypeCheck(CompilerContext* context)
{
	Type* stru = 0;
	std::string fname;
	if (auto name = dynamic_cast<NameExpression*>(left))
	{
		//ok handle what to do if im an index expression
		fname = name->GetName();

		//need to use the template stuff how to get it working with index expressions tho???
	}
	else if (auto index = dynamic_cast<IndexExpression*>(left))
	{
		//im a struct yo
		fname = index->member.text;
		stru = index->GetBaseType(context, true);
		//stru->Load(context->root);
		//assert(stru->loaded);
		//llvm::Value* self = index->GetBaseElementPointer(context).val;
		if (index->token.type == TokenType::Pointy)
		{
			if (stru->type != Types::Pointer && stru->type != Types::Array)
				context->root->Error("Cannot dereference type " + stru->ToString(), this->open);

			stru = stru->base;
			//self = context->root->builder.CreateLoad(self);
		}
		
		//push in the this pointer argument kay
		//argsv.push_back(CValue(stru->GetPointerType(), self));
	}
	else
	{
		throw 7;
		/*auto lhs = this->left->Compile(context);
		if (lhs.type->type != Types::Function)
		context->root->Error("Cannot call non-function", *context->current_token);

		std::vector<llvm::Value*> argts;
		for (auto ii : *this->args)
		{
		auto val = ii->Compile(context);
		argts.push_back(val.val);
		}
		return lhs.type->function;*/// CValue(lhs.type->function->return_type, context->root->builder.CreateCall(lhs.val, argts));
	}
	std::vector<Type*> arg;
	if (stru)
		arg.push_back(stru->GetPointerType());

	for (auto ii : *args)
		arg.push_back(ii.first->TypeCheck(context));
	
	auto fun = context->GetMethod(fname, arg, stru);
	if (fun == 0)
	{
		//check variables
		context->CurrentToken(&this->open);
		auto var = context->TCGetVariable(fname);
		if (var->type == Types::Pointer && var->base->type == Types::Struct && var->base->data->template_base && var->base->data->template_base->name == "function")
			return var->base->data->members.find("T")->second.ty->function->return_type;
		else if (var->base->type == Types::Function)
			return var->base->function->return_type;

		context->root->Error("Cannot call method '" + fname + "'", this->open);
	}

	if (fun->arguments.size() == arg.size() + 1)
	{
		//its a constructor or something
		return fun->arguments[0].first->base;
		//throw 7;
	}

	//keep working on this, dont forget constructors
	//auto left = this->left->TypeCheck(context);
	//todo: check args


	//throw 7;
	return fun->return_type;
}

CValue YieldExpression::Compile(CompilerContext* context)
{
	//first make sure we are in a generator...
	if (context->function->is_generator == false)
		context->root->Error("Cannot use yield outside of a generator!", this->token);

	//create a new block for after the yield
	auto bb = llvm::BasicBlock::Create(llvm::getGlobalContext(), "yield");
	context->function->f->getBasicBlockList().push_back(bb);

	//add the new block to the indirect branch list
	context->function->generator.ibr->addDestination(bb);

	//store the current location into the generator context
	auto data = context->Load("_context");
	auto br = context->root->builder.CreateGEP(data.val, { context->root->builder.getInt32(0), context->root->builder.getInt32(0) });
	auto ba = llvm::BlockAddress::get(bb);
	context->root->builder.CreateStore(ba, br);

	if (this->right)
	{
		//compile the yielded value
		auto value = right->Compile(context);

		//store result into the generator context
		value = context->DoCast(data.type->base->data->struct_members[1].type, value);//cast to the correct type
		br = context->root->builder.CreateGEP(data.val, { context->root->builder.getInt32(0), context->root->builder.getInt32(1) });
		context->root->builder.CreateStore(value.val, br);
	}

	//return 1 to say we are not finished yielding
	context->root->builder.CreateRet(context->root->builder.getInt1(true));

	//start inserting in new block
	context->root->builder.SetInsertPoint(bb);

	return CValue();
}

CValue MatchExpression::Compile(CompilerContext* context)
{
	CValue val;//first get pointer to union
	auto i = dynamic_cast<NameExpression*>(var);
	auto p = dynamic_cast<IndexExpression*>(var);
	if (i)
		val = context->GetVariable(i->GetName());
	else if (p)
		val = p->GetElementPointer(context);

	if (val.type->base->type != Types::Union)
		context->root->Error("Cannot match with a non-union", token);

	auto endbb = llvm::BasicBlock::Create(llvm::getGlobalContext(), "match.end");

	//from val get the type
	auto key = context->root->builder.CreateGEP(val.val, { context->root->builder.getInt32(0), context->root->builder.getInt32(0) });
	auto sw = context->root->builder.CreateSwitch(context->root->builder.CreateLoad(key), endbb, this->cases.size());

	for (auto ii : this->cases)
	{
		context->PushScope();

		if (ii.type.type == TokenType::Default)
		{
			//add bb for case
			auto bb = llvm::BasicBlock::Create(llvm::getGlobalContext(), "match.case", context->function->f);
			context->root->builder.SetInsertPoint(bb);
			sw->setDefaultDest(bb);

			//build internal
			ii.block->Compile(context);

			//branch to end
			context->root->builder.CreateBr(endbb);
			break;
		}

		int pi = 0;//find what index it is
		for (auto mem : val.type->base->_union->members)
		{
			if (mem->name == ii.type.text)
				break;
			pi++;
		}

		if (pi >= val.type->base->_union->members.size())
			context->root->Error("Type '" + ii.type.text + "' not in union, cannot match to it.", ii.type);

		//add bb for case
		auto bb = llvm::BasicBlock::Create(llvm::getGlobalContext(), "match.case", context->function->f);
		context->root->builder.SetInsertPoint(bb);
		auto i = llvm::ConstantInt::get(llvm::getGlobalContext(), llvm::APInt(32, (uint64_t)pi));
		sw->addCase(i, bb);

		//add local
		auto ptr = context->root->builder.CreateGEP(val.val, { context->root->builder.getInt32(0), context->root->builder.getInt32(1) });
		ptr = context->root->builder.CreatePointerCast(ptr, val.type->base->_union->members[pi]->GetPointerType()->GetLLVMType());
		context->RegisterLocal(ii.name.text, CValue(val.type->base->_union->members[pi]->GetPointerType(), ptr));

		//build internal
		ii.block->Compile(context);

		//need to do this without destructing args
		context->scope->named_values[ii.name.text] = CValue();
		context->PopScope();

		//branch to end
		context->root->builder.CreateBr(endbb);
	}

	//start new basic block
	context->function->f->getBasicBlockList().push_back(endbb);
	context->root->builder.SetInsertPoint(endbb);

	return CValue();
}

CValue CallExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&this->open);
	context->SetDebugLocation(this->open);

	std::vector<CValue> argsv;

	std::string fname;
	Type* stru = 0;
	bool devirtualize = false;
	if (auto name = dynamic_cast<NameExpression*>(left))
	{
		//ok handle what to do if im an index expression
		fname = name->GetName();

		//need to use the template stuff how to get it working with index expressions tho???
	}
	else if (auto index = dynamic_cast<IndexExpression*>(left))
	{
		//im a struct yo
		fname = index->member.text;
		stru = index->GetBaseType(context);
		assert(stru->loaded);
		llvm::Value* self = index->GetBaseElementPointer(context).val;
		if (index->token.type == TokenType::Pointy)
		{
			if (stru->type != Types::Pointer && stru->type != Types::Array)
				context->root->Error("Cannot dereference type " + stru->ToString(), this->open);

			stru = stru->base;
			self = context->root->builder.CreateLoad(self);
		}
		else
			devirtualize = true;//if we arent using -> we dont need a virtual call, right?

		//push in the this pointer argument kay
		argsv.push_back(CValue(stru->GetPointerType(), self));
	}
	else
	{
		//calling a function pointer type
		auto lhs = this->left->Compile(context);
		if (lhs.type->type != Types::Function)
			context->root->Error("Cannot call non-function", *context->current_token);

		std::vector<llvm::Value*> argts;
		for (auto ii : *this->args)
			argts.push_back(ii.first->Compile(context).val);
		return CValue(lhs.type->function->return_type, context->root->builder.CreateCall(lhs.val, argts));
	}

	//need to pass all structs as pointers

	//build arg list
	for (auto ii : *this->args)
		argsv.push_back(ii.first->Compile(context));

	context->CurrentToken(&this->open);
	auto ret = context->Call(fname, argsv, stru, devirtualize);

	//destruct if my parent doesnt use me
	if (ret.type->type == Types::Struct && dynamic_cast<BlockExpression*>(this->parent))
	{
		//need to escalate to a pointer
		auto TheFunction = context->function->f;
		llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
			TheFunction->getEntryBlock().begin());
		auto alloc = TmpB.CreateAlloca(ret.type->GetLLVMType(), 0, "return_pass_tmp");
		context->root->builder.CreateStore(ret.val, alloc);

		context->Destruct(CValue(ret.type->GetPointerType(),alloc), 0);
	}

	return ret;
}

