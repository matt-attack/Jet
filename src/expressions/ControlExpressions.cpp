#include "Expressions.h"
#include "Compiler.h"
#include "Parser.h"
#include "types/Function.h"
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
	context->function->f_->getBasicBlockList().push_back(block);
	context->root->builder.SetInsertPoint(block);
	return CValue(context->root->VoidType, 0);
}

CValue CaseExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&token);

	SwitchExpression* sw = dynamic_cast<SwitchExpression*>(this->parent->parent);
	if (sw == 0)
		context->root->Error("Cannot use case expression outside of a switch!", token);

	//create a new block for this case
	llvm::BasicBlock* block = llvm::BasicBlock::Create(context->context, "case" + value.text);

	//add the case to the switch
	bool is_first = sw->AddCase(context->root->builder.getInt32(std::stol(this->value.text)), block);

	//jump to end block
	if (!is_first)
		context->root->builder.CreateBr(sw->switch_end);

	//start using our new block
	context->function->f_->getBasicBlockList().push_back(block);
	context->root->builder.SetInsertPoint(block);
	return CValue(context->root->VoidType, 0);
}

CValue IfExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&token);

	int pos = 0;
	bool hasElse = this->Else ? this->Else->block->statements.size() > 0 : false;
	llvm::BasicBlock *EndBB = llvm::BasicBlock::Create(context->context, "endif");
	llvm::BasicBlock *ElseBB = 0;
	if (hasElse)
	{
		ElseBB = llvm::BasicBlock::Create(context->context, "else");
	}

	llvm::BasicBlock *NextBB = 0;
	for (auto& ii : this->branches)
	{
		if (NextBB)
			context->root->builder.SetInsertPoint(NextBB);

		auto cond = ii->condition->Compile(context);
		cond = context->DoCast(context->root->BoolType, cond);//try and cast to bool

		llvm::BasicBlock *ThenBB = llvm::BasicBlock::Create(context->context, "then", context->function->f_);
		NextBB = pos == (branches.size() - 1) ? (hasElse ? ElseBB : EndBB) : llvm::BasicBlock::Create(context->context, "elseif", context->function->f_);

		context->root->builder.CreateCondBr(cond.val, ThenBB, NextBB);

		//statement body
		context->root->builder.SetInsertPoint(ThenBB);
		ii->block->Compile(context);
		context->root->builder.CreateBr(EndBB);//branch to end

		pos++;
	}

	if (hasElse)
	{
		context->function->f_->getBasicBlockList().push_back(ElseBB);
		context->root->builder.SetInsertPoint(ElseBB);

		this->Else->block->Compile(context);
		context->root->builder.CreateBr(EndBB);
	}

	context->function->f_->getBasicBlockList().push_back(EndBB);
	context->root->builder.SetInsertPoint(EndBB);

	return CValue(context->root->VoidType, 0);
}

CValue SwitchExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&token);

	CValue value = this->var->Compile(context);
	if (value.type->type != Types::Int)// todo need to make this work with other integer types
	{
		context->root->Error("Argument to Case Statement Must Be an Integer", token);
	}

	this->switch_end = llvm::BasicBlock::Create(context->context, "switchend");

	//look for all case and default expressions
	std::vector < CaseExpression* > cases;
	for (auto expr : this->block->statements)
	{
		auto Case = dynamic_cast<CaseExpression*>(expr);
		if (Case)
		{
			cases.push_back(Case);
		}
		//add default parser and expression
		else if (auto def = dynamic_cast<DefaultExpression*>(expr))
		{
			//do default
			if (this->def)
				context->root->Error("Multiple defaults defined for the same switch!", token);
			this->def = llvm::BasicBlock::Create(context->context, "switchdefault");
		}
	}

	bool no_def = def ? false : true;
	if (def == 0)
	{
		//create default block at end if there isnt one
		this->def = llvm::BasicBlock::Create(context->context, "switchdefault");
	}

	//create the switch instruction
	this->sw = context->root->builder.CreateSwitch(value.val, def, cases.size());

	//compile the block
	this->block->Compile(context);

	context->root->builder.CreateBr(this->switch_end);

	if (no_def)
	{
		//insert and create a dummy default
		context->function->f_->getBasicBlockList().push_back(def);
		context->root->builder.SetInsertPoint(def);
		context->root->builder.CreateBr(this->switch_end);
	}

	//start using end
	context->function->f_->getBasicBlockList().push_back(this->switch_end);
	context->root->builder.SetInsertPoint(this->switch_end);

	return CValue(context->root->VoidType, 0);
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
	
    bool is_constructor;
	Function* fun = 0;//context->GetMethod(fname, arg, stru, is_constructor);
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

	if (fun->arguments_.size() == arg.size() + 1)
	{
		//its a constructor or something
		return fun->arguments_[0].first->base;
	}

	//keep working on this, dont forget constructors
	//auto left = this->left->TypeCheck(context);
	//todo: check args


	//throw 7;
	return fun->return_type_;
}

CValue YieldExpression::Compile(CompilerContext* context)
{
	//first make sure we are in a generator...
	if (context->function->is_generator_ == false)
		context->root->Error("Cannot use yield outside of a generator!", this->token);

	//create a new block for after the yield
	auto bb = llvm::BasicBlock::Create(context->context, "yield");
	context->function->f_->getBasicBlockList().push_back(bb);

	//add the new block to the generator's indirect branch list
	context->function->generator_.ibr->addDestination(bb);

	//store the current location into the generator context so we can jump back
	auto data = context->Load("_context");
	auto br = context->root->builder.CreateGEP(data.val, { context->root->builder.getInt32(0), context->root->builder.getInt32(0) });
	auto ba = llvm::BlockAddress::get(bb);
	context->root->builder.CreateStore(ba, br);

	if (this->right)
	{
		//compile the yielded value
		auto value = right->Compile(context);

		auto dest_type = data.type->base->data->struct_members[1].type;

        if (!value.val)
        {
            value.val = context->root->builder.CreateLoad(value.pointer, "autodereference");
        }

		//store result into the generator context
		value = context->DoCast(dest_type, value);//cast to the correct type
		br = context->root->builder.CreateGEP(data.val, { context->root->builder.getInt32(0), context->root->builder.getInt32(1) });
		context->root->builder.CreateStore(value.val, br);
	}

	//return 1 to say the function isnt done yet, we havent returned, just yielded
	context->root->builder.CreateRet(context->root->builder.getInt1(true));

	//start inserting in new block
	context->root->builder.SetInsertPoint(bb);

	return CValue(context->root->VoidType, 0);
}

CValue MatchExpression::Compile(CompilerContext* context)
{
	CValue val(0,0);//first get pointer to union
	auto i = dynamic_cast<NameExpression*>(var);
	auto p = dynamic_cast<IndexExpression*>(var);
	if (i)
		val = context->GetVariable(i->GetName());
	else if (p)
		val = p->GetElement(context);

	if (!val.type || val.type->type != Types::Union)
    {
        if (!val.type)
        {
            val = var->Compile(context);
        }
        // If its not a union, we static branch based on the type
        for (auto ii : this->cases)
	    {
	    	if (ii.type.type == TokenType::Default)
	    	{
	    		continue;
	    	}
	    	
            // if its a match, compile it then return
            Type* ty = context->root->LookupType(ii.type.text);

            if (ty == val.type)
            {
                context->PushScope();

                // add the variable, if it doesnt match
                if (!i || i->GetName() != ii.name.text)
                {
                    context->RegisterLocal(ii.name.text, val);
                }

                ii.block->Compile(context);

		        context->PopScope();

                return CValue(context->root->VoidType, 0);
            }
        }

        // otherwise, try and compile the default
        for (auto ii : this->cases)
	    {
            if (ii.type.type == TokenType::Default)
            {
                ii.block->Compile(context);

                return CValue(context->root->VoidType, 0);
            }
        }

        return CValue(context->root->VoidType, 0);
    }

	auto endbb = llvm::BasicBlock::Create(context->context, "match.end");

	//from val get the type
	auto key = context->root->builder.CreateGEP(val.pointer, { context->root->builder.getInt32(0), context->root->builder.getInt32(0) });
	auto sw = context->root->builder.CreateSwitch(context->root->builder.CreateLoad(key), endbb, this->cases.size());

    const Type* union_type = val.type;
	for (auto ii : this->cases)
	{
		context->PushScope();

		if (ii.type.type == TokenType::Default)
		{
			//add bb for case
			auto bb = llvm::BasicBlock::Create(context->context, "match.case", context->function->f_);
			context->root->builder.SetInsertPoint(bb);
			sw->setDefaultDest(bb);

			//build internal
			ii.block->Compile(context);

			//branch to end
			context->root->builder.CreateBr(endbb);
			continue;
		}

		unsigned int pi = 0;//find what index it is
		for (auto mem : union_type->_union->members)
		{
			if (mem->name == ii.type.text)
				break;
			pi++;
		}

		if (pi >= union_type->_union->members.size())
		{
			context->root->Error("Type '" + ii.type.text + "' not in union, cannot match to it.", ii.type);
		}

		//add bb for case
		auto bb = llvm::BasicBlock::Create(context->context, "match.case", context->function->f_);
		context->root->builder.SetInsertPoint(bb);
		auto i = llvm::ConstantInt::get(context->context, llvm::APInt(32, (uint64_t)pi));
		sw->addCase(i, bb);

		//add local
		auto ptr = context->root->builder.CreateGEP(val.pointer, { context->root->builder.getInt32(0), context->root->builder.getInt32(1) });
		ptr = context->root->builder.CreatePointerCast(ptr, union_type->_union->members[pi]->GetPointerType()->GetLLVMType());
		context->RegisterLocal(ii.name.text, CValue(union_type->_union->members[pi], 0, ptr));

		//build internal
		ii.block->Compile(context);

		//need to do this without destructing args
		context->scope->named_values.insert( {ii.name.text, CValue(context->root->VoidType, 0) } );
		context->PopScope();

		//branch to end
		context->root->builder.CreateBr(endbb);
	}

	//start new basic block
	context->function->f_->getBasicBlockList().push_back(endbb);
	context->root->builder.SetInsertPoint(endbb);

	return CValue(context->root->VoidType, 0);
}

CValue CallExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&this->open);
	context->SetDebugLocation(this->open);

	std::vector<FunctionArgument> argsv;

	Token fname;
	Type* stru = 0;
	bool devirtualize = false;
    bool is_const = false;
	if (auto name = dynamic_cast<NameExpression*>(left))
	{
		//ok handle what to do if im an index expression
		fname = name->token;

        CValue var = context->GetVariable(fname.text, false);

        if (var.type->type == Types::Struct)
        {
          // hack for lambdas, fixme later
          if (!(var.type->data->template_base && var.type->data->template_base->name == "function"))
          {
            stru = var.type;
            argsv.push_back({CValue(stru->GetPointerType(), var.pointer), 0});
            fname.text = "()";
          }
        }

		//need to use the template stuff how to get it working with index expressions tho???
	}
	else if (auto index = dynamic_cast<IndexExpression*>(left))
	{
		//im a struct yo
		fname = index->member;

		auto left = index->GetBaseElement(context);
        is_const = left.is_const;

        // now find the struct (dereference up to one time)
        if (left.type->type == Types::Struct)
        {
          // okay, pointer must be filled out
          assert(left.pointer);
          stru = left.type;

          argsv.push_back({CValue(stru->GetPointerType(), left.pointer), 0});
        }
        else if (left.type->type == Types::Pointer &&
                 left.type->base->type == Types::Struct)
        {
          is_const = false;
          auto val = left.val;
          if (!val)
          {
            val = context->root->builder.CreateLoad(left.pointer);
          }
          assert(val);
          stru = left.type->base;

          argsv.push_back({CValue(left.type, val), 0});
        }
        else
        {
          context->root->Error("Could not calculate this pointer", context->current_token);
        }
	}
	else
	{
		//calling a function pointer type
		auto lhs = this->left->Compile(context);
		if (lhs.type->type != Types::Function)
			context->root->Error("Cannot call non-function", context->current_token);

		std::vector<FunctionArgument> argts;
		for (auto ii : *this->args)
			argts.push_back({ii.first->Compile(context), ii.first});

        return lhs.type->function->Call(context, lhs.val, argts);
	}

	//build arg list
	for (auto ii : *this->args)
		argsv.push_back({ii.first->Compile(context), ii.first});

	context->CurrentToken(this->GetTokenRange());
	auto ret = context->Call(fname, argsv, stru, devirtualize, is_const);

    // add any returned struct to the destruct queue to be removed when this statement ends
    // that way it doesnt leak
    if (ret.type->type == Types::Struct)
    {
        context->DestructLater(ret);
    }

	return ret;
}

