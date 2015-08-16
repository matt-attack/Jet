#include "Expressions.h"
#include "Compiler.h"
#include "Parser.h"

using namespace Jet;

#include <llvm\IR\IRBuilder.h>
#include <llvm\IR\Module.h>
#include <llvm\IR\LLVMContext.h>
#include <llvm\IR\DIBuilder.h>
#include <llvm/IR/DerivedTypes.h>

CValue PrefixExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&this->_operator);

	if (this->_operator.type == TokenType::BAnd)
	{
		auto i = dynamic_cast<NameExpression*>(right);
		auto p = dynamic_cast<IndexExpression*>(right);
		if (i)
		{
			auto var = context->GetVariable(i->GetName());
			return CValue(var.type, var.val);
		}
		else if (p)
		{
			auto var = p->GetElementPointer(context);
			return CValue(var.type, var.val);
		}
		context->parent->Error("Not Implemented", this->_operator);
	}

	auto rhs = right->Compile(context);

	auto res = context->UnaryOperation(this->_operator.type, rhs);
	//store here
	//only do this for ++and--
	if (this->_operator.type == TokenType::Increment || this->_operator.type == TokenType::Decrement)
		if (auto storable = dynamic_cast<IStorableExpression*>(this->right))
			storable->CompileStore(context, res);

	return res;
}

CValue SizeofExpression::Compile(CompilerContext* context)
{
	auto t = context->parent->LookupType(type.text);
	auto null = llvm::ConstantPointerNull::get(GetType(t)->getPointerTo());
	auto ptr = context->parent->builder.CreateGEP(null, context->parent->builder.getInt32(1));
	ptr = context->parent->builder.CreatePtrToInt(ptr, GetType(context->parent->IntType), "sizeof");
	return CValue(context->parent->IntType, ptr);// context->DoCast(t, right->Compile(context), true);
}

CValue PostfixExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&this->_operator);
	auto lhs = left->Compile(context);

	auto res = context->UnaryOperation(this->_operator.type, lhs);

	//only do this for ++ and --
	if (this->_operator.type == TokenType::Increment || this->_operator.type == TokenType::Decrement)
		if (auto storable = dynamic_cast<IStorableExpression*>(this->left))	//store here
			storable->CompileStore(context, res);

	return lhs;
}

CValue IndexExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&token);
	context->SetDebugLocation(this->token);
	auto loc = this->GetElementPointer(context);
	if (loc.type->type == Types::Function)
		return loc;
	return CValue(loc.type->base, context->parent->builder.CreateLoad(loc.val));
}

Type* IndexExpression::GetBaseType(CompilerContext* context)
{
	auto p = dynamic_cast<NameExpression*>(left);
	auto i = dynamic_cast<IndexExpression*>(left);

	if (p)
		return context->GetVariable(p->GetName()).type->base;
	else if (i)
		return i->GetType(context);

	context->parent->Error("wat", token);
}

Type* IndexExpression::GetBaseType(Compilation* compiler)
{
	auto p = dynamic_cast<NameExpression*>(left);
	auto i = dynamic_cast<IndexExpression*>(left);

	if (p)
	{
		//look for neareast scope
		auto current = this->Parent;
		do
		{
			auto scope = dynamic_cast<ScopeExpression*>(current);
			if (scope)
			{
				//search for it now
				auto curscope = scope->scope;
				do
				{
					auto res = curscope->named_values.find(p->GetName());
					if (res != curscope->named_values.end())
						return res->second.type;
				} while (curscope = curscope->prev);
				break;
			}
		} while (current = current->Parent);
	}
	//return context->GetVariable(p->GetName()).type;
	else if (i)
	{
		compiler->Error("todo", token);//return i->GetType(context);
	}

	compiler->Error("wat", token);
}

CValue IndexExpression::GetBaseElementPointer(CompilerContext* context)
{
	auto p = dynamic_cast<NameExpression*>(left);
	auto i = dynamic_cast<IndexExpression*>(left);

	if (p)
		return context->GetVariable(p->GetName());
	else if (i)
		return i->GetElementPointer(context);

	context->parent->Error("wat", token);
}

Type* IndexExpression::GetType(CompilerContext* context)
{
	auto p = dynamic_cast<NameExpression*>(left);
	auto i = dynamic_cast<IndexExpression*>(left);

	auto string = dynamic_cast<StringExpression*>(index);
	if (p || i)
	{
		CValue lhs;
		if (p)
			lhs = context->GetVariable(p->GetName());
		else if (i)
			lhs = i->GetElementPointer(context);

		if (string && lhs.type->type == Types::Struct)
		{
			int index = 0;
			for (; index < lhs.type->data->struct_members.size(); index++)
			{
				if (lhs.type->data->struct_members[index].name == string->GetValue())
					break;
			}

			if (index >= lhs.type->data->struct_members.size())
				context->parent->Error("Struct Member '" + string->GetValue() + "' of Struct '" + lhs.type->data->name + "' Not Found", this->token);

			return lhs.type->data->struct_members[index].type;
		}
		else if (this->token.type == TokenType::Dot && this->member.text.length() && lhs.type->type == Types::Pointer && lhs.type->base->type == Types::Struct)
		{
			int index = 0;
			for (; index < lhs.type->base->data->struct_members.size(); index++)
			{
				if (lhs.type->base->data->struct_members[index].name == this->member.text)
					break;
			}

			if (index >= lhs.type->base->data->struct_members.size())
				context->parent->Error("Struct Member '" + this->member.text + "' of Struct '" + lhs.type->base->data->name + "' Not Found", this->member);

			return lhs.type->base->data->struct_members[index].type;
		}
		else if (this->token.type == TokenType::Pointy && this->member.text.length() && lhs.type->type == Types::Pointer && lhs.type->base->type == Types::Pointer && lhs.type->base->base->type == Types::Struct)
		{
			int index = 0;
			for (; index < lhs.type->base->base->data->struct_members.size(); index++)
			{
				if (lhs.type->base->base->data->struct_members[index].name == this->member.text)
					break;
			}

			if (index >= lhs.type->base->base->data->struct_members.size())
				context->parent->Error("Struct Member '" + this->member.text + "' of Struct '" + lhs.type->base->base->data->name + "' Not Found", this->member);

			return lhs.type->base->base->data->struct_members[index].type;
		}
		else if ((lhs.type->type == Types::Array || lhs.type->type == Types::Pointer) && string == 0)//or pointer!!(later)
		{
			return lhs.type->base->base;
		}
	}
}

CValue IndexExpression::GetElementPointer(CompilerContext* context)
{
	auto p = dynamic_cast<NameExpression*>(left);
	auto i = dynamic_cast<IndexExpression*>(left);

	if (index == 0 && this->token.type == TokenType::Pointy)
	{
		CValue lhs;
		if (p)
			lhs = context->GetVariable(p->GetName());
		else if (i)
			lhs = i->GetElementPointer(context);

		if (lhs.type->type == Types::Pointer && lhs.type->base->type == Types::Pointer && lhs.type->base->base->type == Types::Struct)
		{
			lhs.val = context->parent->builder.CreateLoad(lhs.val);

			auto type = lhs.type->base->base;
			int index = 0;
			for (; index < type->data->struct_members.size(); index++)
			{
				if (type->data->struct_members[index].name == this->member.text)
					break;
			}
			if (index >= type->data->struct_members.size())
			{
				//check methods
				auto method = type->GetMethod(this->member.text, {}, context, true);
				if (method == 0)
					context->parent->Error("Struct Member '" + this->member.text + "' of Struct '" + type->data->name + "' Not Found", this->member);
				return CValue(method->GetType(context->parent), method->f);
			}

			std::vector<llvm::Value*> iindex = { context->parent->builder.getInt32(0), context->parent->builder.getInt32(index) };

			auto loc = context->parent->builder.CreateGEP(lhs.val, iindex, "index");
			return CValue(type->data->struct_members[index].type->GetPointerType(context), loc);
		}

		context->parent->Error("unimplemented!", this->token);
	}
	else if (p || i)
	{
		CValue lhs;
		if (p)
			lhs = context->GetVariable(p->GetName());
		else if (i)
			lhs = i->GetElementPointer(context);

		if (this->member.text.length() && lhs.type->type == Types::Pointer && lhs.type->base->type == Types::Struct)
		{
			int index = 0;
			for (; index < lhs.type->base->data->struct_members.size(); index++)
			{
				if (lhs.type->base->data->struct_members[index].name == this->member.text)
					break;
			}
			if (index >= lhs.type->base->data->struct_members.size())
			{
				//check methods
				auto method = lhs.type->base->GetMethod(this->member.text, {}, context, true);
				if (method == 0)
					context->parent->Error("Struct Member '" + this->member.text + "' of Struct '" + lhs.type->base->data->name + "' Not Found", this->member);
				return CValue(method->GetType(context->parent), method->f);
			}
			std::vector<llvm::Value*> iindex = { context->parent->builder.getInt32(0), context->parent->builder.getInt32(index) };

			auto loc = context->parent->builder.CreateGEP(lhs.val, iindex, "index");
			return CValue(lhs.type->base->data->struct_members[index].type->GetPointerType(context), loc);
		}
		else if (lhs.type->type == Types::Pointer && lhs.type->base->type == Types::Array && this->member.text.length() == 0)//or pointer!!(later)
		{
			std::vector<llvm::Value*> iindex = { context->parent->builder.getInt32(0), context->DoCast(context->parent->IntType, index->Compile(context)).val };

			auto loc = context->parent->builder.CreateGEP(lhs.val, iindex, "index");

			return CValue(lhs.type/*->base*/, loc);
		}
		else if (lhs.type->type == Types::Pointer && lhs.type->base->type == Types::Pointer && this->member.text.length() == 0)//or pointer!!(later)
		{
			std::vector<llvm::Value*> iindex = { context->DoCast(context->parent->IntType, index->Compile(context)).val };

			//loadme!!!
			lhs.val = context->parent->builder.CreateLoad(lhs.val);
			//llllload my index
			auto loc = context->parent->builder.CreateGEP(lhs.val, iindex, "index");

			return CValue(lhs.type->base, loc);
		}
		else if (lhs.type->type == Types::Struct && this->member.text.length() == 0)
		{
			context->parent->Error("Indexing Structs Not Implemented", this->token);
		}
		context->parent->Error("Cannot index type '" + lhs.type->ToString() + "'", this->token);
	}

	context->parent->Error("Unimplemented", this->token);
}


void IndexExpression::CompileStore(CompilerContext* context, CValue right)
{
	context->CurrentToken(&token);
	context->SetDebugLocation(this->token);
	auto loc = this->GetElementPointer(context);

	right = context->DoCast(loc.type->base, right);
	context->parent->builder.CreateStore(right.val, loc.val);
}

CValue StringExpression::Compile(CompilerContext* context)
{
	return context->String(this->value);
}

/*void NullExpression::Compile(CompilerContext* context)
{
context->Null();

//pop off if we dont need the result
if (dynamic_cast<BlockExpression*>(this->Parent))
context->Pop();
}*/

CValue NumberExpression::Compile(CompilerContext* context)
{
	bool isint = true;
	bool ishex = false;
	for (int i = 0; i < this->token.text.length(); i++)
	{
		if (this->token.text[i] == '.')
			isint = false;
		else if (!(this->token.text[i] <= '9' && this->token.text[i] >= '0'))
			ishex = true;
	}
	//ok, lets get the type from what kind of constant it is
	//get type from the constant
	//this is pretty terrible, come back later
	if (ishex)
		return context->Integer(std::stoi(this->token.text, 0, 16));
	else if (isint)
		return context->Integer(std::stoi(this->token.text));
	else
		return context->Float(this->value);
}

CValue AssignExpression::Compile(CompilerContext* context)
{
	context->SetDebugLocation(this->token);

	if (auto storable = dynamic_cast<IStorableExpression*>(this->left))
	{
		auto r = right->Compile(context);

		storable->CompileStore(context, r);
	}

	return CValue();
}

CValue CallExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&this->token);
	context->SetDebugLocation(this->token);

	std::vector<CValue> argsv;

	std::string fname;
	Type* stru = 0;
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

		llvm::Value* self;
		if (index->token.type == TokenType::Pointy)
		{
			stru = stru->base;
			self = context->parent->builder.CreateLoad(index->GetBaseElementPointer(context).val);
		}
		else
		{
			self = index->GetBaseElementPointer(context).val;
		}

		//push in the this pointer argument kay
		argsv.push_back(CValue(stru->GetPointerType(context), self));
	}
	else
	{
		auto lhs = this->left->Compile(context);
		if (lhs.type->type != Types::Function)
			context->parent->Error("Cannot call non-function", *context->current_token);

		std::vector<llvm::Value*> argts;
		for (auto ii : *this->args)
		{
			auto val = ii->Compile(context);
			argts.push_back(val.val);
		}
		return CValue(lhs.type->function->return_type, context->parent->builder.CreateCall(lhs.val, argts));
	}

	//build arg list
	for (auto ii : *this->args)
		argsv.push_back(ii->Compile(context));

	return context->Call(fname, argsv, stru);
}

CValue NameExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&token);

	return context->Load(token.text);
}

CValue OperatorAssignExpression::Compile(CompilerContext* context)
{
	//try and cast right side to left
	auto lhs = this->left->Compile(context);
	auto rhs = this->right->Compile(context);
	rhs = context->DoCast(lhs.type, rhs);

	context->CurrentToken(&token);
	context->SetDebugLocation(token);
	auto res = context->BinaryOperation(token.type, lhs, rhs);

	//insert store here
	if (auto storable = dynamic_cast<IStorableExpression*>(this->left))
		storable->CompileStore(context, res);

	return CValue();
}

CValue OperatorExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&this->_operator);
	context->SetDebugLocation(this->_operator);
	if (this->_operator.type == TokenType::And)
	{
		auto else_block = llvm::BasicBlock::Create(llvm::getGlobalContext(), "land.shortcircuitelse");
		auto end_block = llvm::BasicBlock::Create(llvm::getGlobalContext(), "land.endshortcircuit");
		auto cur_block = context->parent->builder.GetInsertBlock();

		auto cond = this->left->Compile(context);
		cond = context->DoCast(context->parent->BoolType, cond);
		context->parent->builder.CreateCondBr(cond.val, else_block, end_block);

		context->f->getBasicBlockList().push_back(else_block);
		context->parent->builder.SetInsertPoint(else_block);
		auto cond2 = this->right->Compile(context);

		cond2 = context->DoCast(context->parent->BoolType, cond2);
		context->parent->builder.CreateBr(end_block);

		context->f->getBasicBlockList().push_back(end_block);
		context->parent->builder.SetInsertPoint(end_block);
		auto phi = context->parent->builder.CreatePHI(GetType(cond.type), 2, "land");
		phi->addIncoming(cond.val, cur_block);
		phi->addIncoming(cond2.val, else_block);

		return CValue(context->parent->BoolType, phi);
	}

	if (this->_operator.type == TokenType::Or)
	{
		auto else_block = llvm::BasicBlock::Create(llvm::getGlobalContext(), "lor.shortcircuitelse");
		auto end_block = llvm::BasicBlock::Create(llvm::getGlobalContext(), "lor.endshortcircuit");
		auto cur_block = context->parent->builder.GetInsertBlock();

		auto cond = this->left->Compile(context);
		cond = context->DoCast(context->parent->BoolType, cond);
		context->parent->builder.CreateCondBr(cond.val, end_block, else_block);

		context->f->getBasicBlockList().push_back(else_block);
		context->parent->builder.SetInsertPoint(else_block);
		auto cond2 = this->right->Compile(context);
		cond2 = context->DoCast(context->parent->BoolType, cond2);
		context->parent->builder.CreateBr(end_block);

		context->f->getBasicBlockList().push_back(end_block);
		context->parent->builder.SetInsertPoint(end_block);

		auto phi = context->parent->builder.CreatePHI(GetType(cond.type), 2, "lor");
		phi->addIncoming(cond.val, cur_block);
		phi->addIncoming(cond2.val, else_block);
		return CValue(context->parent->BoolType, phi);
	}

	auto lhs = this->left->Compile(context);
	auto rhs = this->right->Compile(context);
	rhs = context->DoCast(lhs.type, rhs);

	return context->BinaryOperation(this->_operator.type, lhs, rhs);
}

unsigned int uuid = 5;
std::string FunctionExpression::GetRealName()
{
	std::string fname;
	if (name.text.length() > 0)
		fname = name.text;
	else
		fname = "_lambda_id_" + std::to_string(uuid++);
	auto Struct = dynamic_cast<StructExpression*>(this->Parent);
	if (this->Struct.text.length() > 0)
		return "__" + this->Struct.text + "_" + fname;
	else
		return Struct ? "__" + Struct->GetName() + "_" + fname : fname;
}

CValue FunctionExpression::Compile(CompilerContext* context)
{
	auto Struct = dynamic_cast<StructExpression*>(this->Parent) ? dynamic_cast<StructExpression*>(this->Parent)->GetName() : this->Struct.text;

	//need to not compile if template or trait
	if (this->templates)
	{
		//context->parent->Error("Not implemented!", token);
		for (auto ii : *this->templates)//make sure traits are valid
		{
			auto iter = context->parent->traits.find(ii.first.text);
			if (iter == context->parent->traits.end() || iter->second->valid == false)
				context->parent->Error("Trait '" + ii.first.text + "' is not defined", ii.first);
		}
		return CValue();
	}

	if (Struct.length())
	{
		auto iter = context->parent->LookupType(Struct);
		if (iter->type == Types::Trait)
			return CValue();
	}

	return this->DoCompile(context);
}

CValue FunctionExpression::DoCompile(CompilerContext* context)
{
	context->CurrentToken(&token);

	bool is_lambda = name.text.length() == 0;

	//build list of types of vars
	std::vector<std::pair<Type*, std::string>> argsv;
	auto Struct = dynamic_cast<StructExpression*>(this->Parent) ? dynamic_cast<StructExpression*>(this->Parent)->GetName() : this->Struct.text;

	if (Struct.length() > 0)
	{   //im a member function
		//insert first arg, which is me
		auto type = context->parent->LookupType(Struct + "*");

		argsv.push_back({ type, "this" });
	}

	auto rp = context->parent->builder.GetInsertBlock();
	auto dp = context->parent->builder.getCurrentDebugLocation();
	if (is_lambda)
	{
		//get parent
		auto call = dynamic_cast<CallExpression*>(this->Parent);

		//find my type
		if (call)
		{
			//look for me in the args
			if (call->left == this)
				context->parent->Error("Cannot imply type of lambda with the args of its call", *context->current_token);

			int i = dynamic_cast<NameExpression*>(call->left) ? 0 : 1;
			for (; i < call->args->size(); i++)
			{
				if ((*call->args)[i] == this)
					break;
			}
			CValue fun = call->left->Compile(context);
			Type* type = fun.type->function->args[i];

			if (type->type == Types::Function)
			{
				//do type inference
				int i2 = 0;
				for (auto& ii : *this->args)
				{
					if (ii.first.length() == 0)//do type inference
						ii.first = type->function->args[i2]->ToString();
					i2++;
				}

				if (this->ret_type.text.length() == 0)//infer return type
					this->ret_type.text = type->function->return_type->ToString();
			}
		}
		else
		{
			for (auto ii : *this->args)
				if (ii.first.length() == 0)
					context->parent->Error("Lambda type inference only implemented for function calls", *context->current_token);
			if (this->ret_type.text.length() == 0)
				context->parent->Error("Lambda type inference only implemented for function calls", *context->current_token);
		}
	}

	for (auto ii : *this->args)
		argsv.push_back({ context->parent->LookupType(ii.first), ii.second });

	context->CurrentToken(&this->ret_type);
	auto ret = context->parent->LookupType(this->ret_type.text);

	CompilerContext* function = context->AddFunction(this->GetRealName(), ret, argsv, Struct.length() > 0 ? true : false);// , this->varargs);

	context->parent->current_function = function;
	function->function->context = function;
	function->function->Load(context->parent);
	function->SetDebugLocation(this->token);

	//alloc args
	auto AI = function->f->arg_begin();
	for (unsigned Idx = 0, e = argsv.size(); Idx != e; ++Idx, ++AI) {
		// Create an alloca for this variable.
		auto aname = argsv[Idx].second;

		llvm::IRBuilder<> TmpB(&function->f->getEntryBlock(), function->f->getEntryBlock().begin());
		auto Alloca = TmpB.CreateAlloca(GetType(argsv[Idx].first), 0, aname);
		// Store the initial value into the alloca.
		function->parent->builder.CreateStore(AI, Alloca);

		AI->setName(aname);

		auto D = context->parent->debug->createLocalVariable(llvm::dwarf::DW_TAG_arg_variable, function->function->scope, aname, context->parent->debug_info.file, this->token.line,
			argsv[Idx].first->GetDebugType(context->parent));

		llvm::Instruction *Call = context->parent->debug->insertDeclare(
			Alloca, D, context->parent->debug->createExpression(), llvm::DebugLoc::get(this->token.line, this->token.column, function->function->scope), Alloca);
		Call->setDebugLoc(llvm::DebugLoc::get(this->token.line, this->token.column, function->function->scope));

		// Add arguments to variable symbol table.
		function->RegisterLocal(aname, CValue(argsv[Idx].first->GetPointerType(context), Alloca));
	}

	block->Compile(function);

	//check for return, and insert one or error if there isnt one
	if (function->f->getBasicBlockList().back().getTerminator() == 0)
		if (this->ret_type.text == "void")
			function->Return(CValue());
		else
			context->parent->Error("Function must return a value!", token);

	context->parent->builder.SetCurrentDebugLocation(dp);
	if (rp)
		context->parent->builder.SetInsertPoint(rp);

	context->parent->current_function = context;
	return CValue(function->function->GetType(context->parent), function->f);
}

void FunctionExpression::CompileDeclarations(CompilerContext* context)
{
	context->CurrentToken(&this->token);
	std::string fname = name.text;

	if (name.text.length() == 0)
		return;//dont compile expression for lambdas

	bool advlookup = true;
	Function* fun = new Function(this->GetRealName());
	fun->expression = this;

	auto str = dynamic_cast<StructExpression*>(this->Parent) ? dynamic_cast<StructExpression*>(this->Parent)->GetName() : this->Struct.text;

	fun->f = 0;
	bool is_trait = false;
	if (str.length() > 0)
	{
		//ok, need to look up type without loading it....
		auto type = context->parent->LookupType(str, false);// AdvanceTypeLookup(str);
		if (type->type == Types::Trait)
		{
			type->Load(context->parent);
			type->trait->extension_methods.insert({ fname, fun });
			is_trait = true;
		}
		//else if (type->type == Types::Invalid)
		//{
		//	context->parent->Error("Type '" + str + "' not defined", *context->current_token);
		//}
		else
		{
			type->data->functions.insert({ fname, fun });
			advlookup = !type->data->template_args.size();
		}
	}
	else
		context->parent->ns->members.insert({ fname, fun });

	if (advlookup)
		/*fun->return_type = */context->parent->AdvanceTypeLookup(&fun->return_type, this->ret_type.text);
	else
		fun->return_type = context->parent->LookupType(this->ret_type.text, false);

	fun->arguments.reserve(this->args->size() + (str.length() ? 1 : 0));
	if (str.length() > 0)//im a member function
	{
		//insert first arg, which is me
		Type* type = 0;
		//if (is_trait == false)
		//	type = context->parent->AdvanceTypeLookup(str + "*");

		fun->arguments.push_back({ type, "this" });
		if (is_trait == false && advlookup)
			context->parent->AdvanceTypeLookup(&fun->arguments.back().first, str + "*");
		else if (is_trait == false)
			fun->arguments.back().first = context->parent->LookupType(str + "*");
	}

	if (this->templates)
	{
		for (auto ii : *this->templates)
			fun->templates.push_back({ context->parent->LookupType(ii.first.text), ii.second.text });
	}


	for (auto ii : *this->args)
	{
		Type* type = 0;
		//if (advlookup)
		//type = context->parent->AdvanceTypeLookup(ii.first);
		if (!advlookup)//else
			type = context->parent->LookupType(ii.first);

		fun->arguments.push_back({ type, ii.second });
		if (advlookup)
			/*type = */context->parent->AdvanceTypeLookup(&fun->arguments.back().first, ii.first);
	}
}

CValue ExternExpression::Compile(CompilerContext* context)
{
	return CValue();
}

void ExternExpression::CompileDeclarations(CompilerContext* context)
{
	std::string fname = name.text;

	Function* fun = new Function(fname);
	/*fun->return_type = */context->parent->AdvanceTypeLookup(&fun->return_type, this->ret_type.text);

	fun->arguments.reserve(this->args->size());
	for (auto ii : *this->args)
	{
		fun->arguments.push_back({ 0, ii.second });
		/*auto type = */context->parent->AdvanceTypeLookup(&fun->arguments.back().first, ii.first);
	}

	fun->f = 0;
	if (Struct.length() > 0)
	{
		fun->name = "__" + Struct + "_" + fname;//mangled name

		//add to struct
		auto ii = context->parent->TryLookupType(Struct);// context->parent->ns->members.find(Struct);
		if (ii == 0)//its new
		{
			context->parent->Error("Not implemented!", token);
			//str = new Type;
			//context->parent->types[this->name] = str;
		}
		else
		{
			if (ii->type != Types::Struct)
				context->parent->Error("Cannot define a function for a type that is not a struct", token);

			ii->data->functions.insert({ fname, fun });
		}
	}
	else
	{
		context->parent->ns->members.insert({ fname, fun });
	}
}

CValue LocalExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&(*_names)[0].second);

	if (this->Parent->Parent == 0)
	{
		//im in global scope
		auto type = context->parent->LookupType(this->_names->front().first.text);
		auto global = context->parent->AddGlobal(this->_names->front().second.text, type);

		//should I add a constructor?
		if (this->_right && this->_right->size() > 0)
		{
			context->parent->Error("Initializing global variables not yet implemented", token);
		}

		return CValue();
	}

	int i = 0;
	for (auto ii : *this->_names) {
		auto aname = ii.second.text;

		llvm::IRBuilder<> TmpB(&context->f->getEntryBlock(), context->f->getEntryBlock().begin());
		Type* type = 0;
		llvm::AllocaInst* Alloca = 0;
		if (ii.first.text.length() > 0)//type was specified
		{
			type = context->parent->LookupType(ii.first.text);

			if (type->type == Types::Array)
				Alloca = TmpB.CreateAlloca(GetType(type), context->parent->builder.getInt32(type->size), aname);
			else
			{
				if (type->GetBaseType()->type == Types::Trait)
					context->parent->Error("Cannot instantiate trait", ii.second);

				auto ty = GetType(type);

				Alloca = TmpB.CreateAlloca(GetType(type), 0, aname);
			}

			auto D = context->parent->debug->createLocalVariable(llvm::dwarf::DW_TAG_auto_variable, context->function->scope, aname, context->parent->debug_info.file, ii.second.line,
				type->GetDebugType(context->parent));

			llvm::Instruction *Call = context->parent->debug->insertDeclare(
				Alloca, D, context->parent->debug->createExpression(), llvm::DebugLoc::get(this->token.line, this->token.column, context->function->scope), Alloca);
			Call->setDebugLoc(llvm::DebugLoc::get(ii.second.line, ii.second.column, context->function->scope));

			// Store the initial value into the alloca.
			if (this->_right)
			{
				auto val = (*this->_right)[i++]->Compile(context);
				//cast it
				val = context->DoCast(type, val);
				context->parent->builder.CreateStore(val.val, Alloca);
			}
		}
		else if (this->_right)
		{
			//infer the type
			auto val = (*this->_right)[i++]->Compile(context);
			type = val.type;
			if (val.type->type == Types::Array)
				Alloca = TmpB.CreateAlloca(GetType(val.type), context->parent->builder.getInt32(val.type->size), aname);
			else
				Alloca = TmpB.CreateAlloca(GetType(val.type), 0, aname);

			llvm::DIFile* unit = context->parent->debug_info.file;

			llvm::DILocalVariable* D = context->parent->debug->createLocalVariable(llvm::dwarf::DW_TAG_arg_variable, context->function->scope, aname, unit, ii.second.line,
				type->GetDebugType(context->parent));

			llvm::Instruction *Call = context->parent->debug->insertDeclare(
				Alloca, D, context->parent->debug->createExpression(), llvm::DebugLoc::get(this->token.line, this->token.column, context->function->scope), Alloca);
			Call->setDebugLoc(llvm::DebugLoc::get(ii.second.line, ii.second.column, context->function->scope));

			context->parent->builder.CreateStore(val.val, Alloca);
		}
		else
			context->parent->Error("Cannot infer type from nothing!", ii.second);

		// Add arguments to variable symbol table.
		context->RegisterLocal(aname, CValue(type->GetPointerType(context), Alloca));

		//construct it!
		if (this->_right == 0 && type->type == Types::Struct)
		{
			//call default construct if it exists
			const std::string& constructor_name = type->data->template_base ? type->data->template_base->name : type->data->name;
			auto iter = type->data->functions.find(constructor_name);
			if (iter != type->data->functions.end())
				context->Call(constructor_name, { CValue(type->GetPointerType(context), Alloca) }, type);
		}
	}

	return CValue();
}

CValue StructExpression::Compile(CompilerContext* context)
{
	if (this->templates)
	{
		//dont compile, just verify that all traits are valid
		for (auto ii : *this->templates)
		{
			auto iter = context->parent->TryLookupType(ii.first.text);
			if (iter == 0 || iter->type != Types::Trait)
				context->parent->Error("Trait '" + ii.first.text + "' is not defined", ii.first);
		}
		return CValue();
	}
	//set the namespace


	for (auto ii : this->members)
	{
		if (ii.type == StructMember::FunctionMember)
			ii.function->Compile(context);
	}

	this->AddConstructors(context);

	return CValue();
}

void StructExpression::AddConstructorDeclarations(Type* str, CompilerContext* context)
{
	bool has_destructor = false;
	bool has_constructor = false;
	auto strname = str->data->template_base ? str->data->template_base->name : str->data->name;

	for (auto ii : this->members)
	{
		if (ii.type == StructMember::FunctionMember)
		{
			if (ii.function->GetName() == strname)
				has_constructor = true;
			else if (ii.function->GetName() == "~" + strname)
				has_destructor = true;
		}
	}
	if (has_constructor == false)
	{
		auto fun = new Function("__" + str->data->name + "_" + strname);//
		fun->return_type = &VoidType;
		fun->arguments = { { context->parent->LookupType(str->data->name + "*", false), "this" } };
		//fun->arguments.push_back({ 0, "this" });// = { { context->parent->AdvanceTypeLookup(str->data->name + "*"), "this" } };
		//context->parent->AdvanceTypeLookup(&fun->arguments.back().first, str->data->name + "*");
		fun->f = 0;
		fun->expression = 0;
		fun->type = (FunctionType*)-1;
		str->data->functions.insert({ strname, fun });//register function in _Struct
	}
	if (has_destructor == false)
	{
		auto fun = new Function("__" + str->data->name + "_~" + strname);//
		fun->return_type = &VoidType;
		fun->arguments = { { context->parent->LookupType(str->data->name + "*", false), "this" } };
		//fun->arguments.push_back({ 0, "this" });// = { { context->parent->AdvanceTypeLookup(str->data->name + "*"), "this" } };
		//context->parent->AdvanceTypeLookup(&fun->arguments.back().first, str->data->name + "*");
		//fun->arguments = { { context->parent->AdvanceTypeLookup(str->data->name + "*"), "this" } };
		fun->f = 0;
		fun->expression = 0;
		fun->type = (FunctionType*)-1;

		str->data->functions.insert({ "~" + strname, fun });
	}
}

void StructExpression::AddConstructors(CompilerContext* context)
{
	auto Struct = this->GetName();

	Type* str = context->parent->LookupType(Struct);//todo get me
	std::string strname = str->data->template_base ? str->data->template_base->name : str->data->name;

	for (auto ii : str->data->functions)
	{
		//need to identify if its a autogenerated fun
		if (ii.second->expression == 0 && ii.second->type == (FunctionType*)-1)//ii.second->f == 0)
		{
			//its probably a constructor/destructor we need to fill in
			auto res = ii.second->name.find('~');
			if (res != -1)
			{
				//destructor
				auto rp = context->parent->builder.GetInsertBlock();
				auto dp = context->parent->builder.getCurrentDebugLocation();

				std::vector<std::pair<Type*, std::string>> argsv;
				argsv.push_back({ context->parent->LookupType(str->data->name + "*"), "this" });

				//context->CurrentToken(&this->ret_type);
				auto ret = context->parent->LookupType("void");

				CompilerContext* function = context->AddFunction(ii.second->name, ret, argsv, Struct.length() > 0 ? true : false);// , this->varargs);
				ii.second->f = function->f;

				context->parent->current_function = function;

				function->function->context = function;
				function->function->Load(context->parent);
				function->SetDebugLocation(this->token);

				//alloc args
				auto AI = function->f->arg_begin();
				for (unsigned Idx = 0, e = argsv.size(); Idx != e; ++Idx, ++AI) {
					// Create an alloca for this variable.
					auto aname = argsv[Idx].second;

					llvm::IRBuilder<> TmpB(&function->f->getEntryBlock(), function->f->getEntryBlock().begin());
					auto Alloca = TmpB.CreateAlloca(GetType(argsv[Idx].first), 0, aname);
					// Store the initial value into the alloca.
					function->parent->builder.CreateStore(AI, Alloca);

					AI->setName(aname);

					auto D = context->parent->debug->createLocalVariable(llvm::dwarf::DW_TAG_arg_variable, function->function->scope, aname, context->parent->debug_info.file, this->token.line,
						argsv[Idx].first->GetDebugType(context->parent));

					llvm::Instruction *Call = context->parent->debug->insertDeclare(
						Alloca, D, context->parent->debug->createExpression(), llvm::DebugLoc::get(this->token.line, this->token.column, function->function->scope), Alloca);
					Call->setDebugLoc(llvm::DebugLoc::get(this->token.line, this->token.column, function->function->scope));

					// Add arguments to variable symbol table.
					function->RegisterLocal(aname, CValue(argsv[Idx].first->GetPointerType(context), Alloca));
				}

				//compile stuff here
				int i = 0;
				for (auto ii : str->data->struct_members)
				{
					if (ii.type->type == Types::Struct)
					{
						//call the constructor
						auto iiname = ii.type->data->template_base ? ii.type->data->template_base->name : ii.type->data->name;
						auto range = ii.type->data->functions.equal_range("~" + iiname);
						for (auto iii = range.first; iii != range.second; iii++)
						{
							if (iii->second->arguments.size() == 1)
							{
								auto myself = function->Load("this");
								std::vector<llvm::Value*> iindex = { context->parent->builder.getInt32(0), context->parent->builder.getInt32(i) };
								//iii f is missing oops
								auto gep = context->parent->builder.CreateGEP(myself.val, iindex, "getmember");
								iii->second->Load(context->parent);
								context->parent->builder.CreateCall(iii->second->f, gep);
							}
						}
					}
					i++;
				}

				//check for return, and insert one or error if there isnt one
				function->Return(CValue());

				context->parent->builder.SetCurrentDebugLocation(dp);
				if (rp)
					context->parent->builder.SetInsertPoint(rp);

				context->parent->current_function = context;

				function->function->Load(context->parent);
			}
			else//constructor
			{
				auto rp = context->parent->builder.GetInsertBlock();
				auto dp = context->parent->builder.getCurrentDebugLocation();

				std::vector<std::pair<Type*, std::string>> argsv;
				argsv.push_back({ context->parent->LookupType(str->data->name + "*"), "this" });

				//context->CurrentToken(&this->ret_type);
				auto ret = context->parent->LookupType("void");

				CompilerContext* function = context->AddFunction(ii.second->name, ret, argsv, Struct.length() > 0 ? true : false);// , this->varargs);
				ii.second->f = function->f;

				context->parent->current_function = function;

				function->function->context = function;
				function->function->Load(context->parent);
				function->SetDebugLocation(this->token);

				//alloc args
				auto AI = function->f->arg_begin();
				for (unsigned Idx = 0, e = argsv.size(); Idx != e; ++Idx, ++AI) {
					// Create an alloca for this variable.
					auto aname = argsv[Idx].second;

					llvm::IRBuilder<> TmpB(&function->f->getEntryBlock(), function->f->getEntryBlock().begin());
					auto Alloca = TmpB.CreateAlloca(GetType(argsv[Idx].first), 0, aname);
					// Store the initial value into the alloca.
					function->parent->builder.CreateStore(AI, Alloca);

					AI->setName(aname);

					llvm::DIFile* unit = context->parent->debug_info.file;

					auto D = context->parent->debug->createLocalVariable(llvm::dwarf::DW_TAG_arg_variable, function->function->scope, aname, unit, this->token.line,
						argsv[Idx].first->GetDebugType(context->parent));

					llvm::Instruction *Call = context->parent->debug->insertDeclare(
						Alloca, D, context->parent->debug->createExpression(), llvm::DebugLoc::get(this->token.line, this->token.column, function->function->scope), Alloca);
					Call->setDebugLoc(llvm::DebugLoc::get(this->token.line, this->token.column, function->function->scope));

					// Add arguments to variable symbol table.
					function->RegisterLocal(aname, CValue(argsv[Idx].first->GetPointerType(context), Alloca));
				}

				//compile stuff here
				int i = 0;
				for (auto ii : str->data->struct_members)
				{
					if (ii.type->type == Types::Struct)
					{
						auto iiname = ii.type->data->template_base ? ii.type->data->template_base->name : ii.type->data->name;

						//call the constructor
						auto range = ii.type->data->functions.equal_range(iiname);
						for (auto iii = range.first; iii != range.second; iii++)
						{
							if (iii->second->arguments.size() == 1)
							{
								auto myself = function->Load("this");
								std::vector<llvm::Value*> iindex = { context->parent->builder.getInt32(0), context->parent->builder.getInt32(i) };
								//iii f is missing oops
								auto gep = context->parent->builder.CreateGEP(myself.val, iindex, "getmember");
								iii->second->Load(context->parent);
								context->parent->builder.CreateCall(iii->second->f, gep);
							}
						}
					}
					i++;
				}

				//check for return, and insert one or error if there isnt one
				function->Return(CValue());

				context->parent->builder.SetCurrentDebugLocation(dp);
				if (rp)
					context->parent->builder.SetInsertPoint(rp);

				context->parent->current_function = context;

				function->function->Load(context->parent);
			}
		}
		else if (ii.second->name == "__" + str->data->name + "_" + strname)
		{
			auto rp = context->parent->builder.GetInsertBlock();
			auto dp = context->parent->builder.getCurrentDebugLocation();

			auto iter = ii.second->f->getBasicBlockList().begin()->begin();
			for (int i = 0; i < ii.second->arguments.size() * 3; i++)
				iter++;
			context->parent->builder.SetInsertPoint(iter);

			int i = 0;
			for (auto iip : str->data->struct_members)
			{
				if (iip.type->type == Types::Struct)
				{
					//call the constructor
					auto iiname = iip.type->data->template_base ? iip.type->data->template_base->name : iip.type->data->name;

					auto range = iip.type->data->functions.equal_range(iiname);
					for (auto iii = range.first; iii != range.second; iii++)
					{
						if (iii->second->arguments.size() == 1)
						{
							auto myself = ii.second->context->Load("this");
							std::vector<llvm::Value*> iindex = { context->parent->builder.getInt32(0), context->parent->builder.getInt32(i) };

							auto gep = context->parent->builder.CreateGEP(myself.val, iindex, "getmember");
							iii->second->Load(context->parent);

							context->parent->builder.CreateCall(iii->second->f, gep);
						}
					}
				}
				i++;
			}

			context->parent->builder.SetCurrentDebugLocation(dp);
			if (rp)
				context->parent->builder.SetInsertPoint(rp);
		}
		else if (ii.second->name == "__" + str->data->name + "_~" + strname)
		{
			auto rp = context->parent->builder.GetInsertBlock();
			auto dp = context->parent->builder.getCurrentDebugLocation();

			auto iter = ii.second->f->getBasicBlockList().begin()->begin();
			for (int i = 0; i < ii.second->arguments.size() * 3; i++)
				iter++;
			context->parent->builder.SetInsertPoint(iter);

			int i = 0;
			for (auto iip : str->data->struct_members)
			{
				if (iip.type->type == Types::Struct)
				{
					//call the destructor
					auto range = iip.type->data->functions.equal_range("~" + iip.type->data->name);
					for (auto iii = range.first; iii != range.second; iii++)
					{
						if (iii->second->arguments.size() == 1)
						{
							auto myself = ii.second->context->Load("this");
							std::vector<llvm::Value*> iindex = { context->parent->builder.getInt32(0), context->parent->builder.getInt32(i) };

							auto gep = context->parent->builder.CreateGEP(myself.val, iindex, "getmember");
							iii->second->Load(context->parent);

							context->parent->builder.CreateCall(iii->second->f, gep);
						}
					}
				}
				i++;
			}

			context->parent->builder.SetCurrentDebugLocation(dp);
			if (rp)
				context->parent->builder.SetInsertPoint(rp);
		}
	}
}

void StructExpression::CompileDeclarations(CompilerContext* context)
{
	//build data about the struct
	//get or add the struct type from the type table
	Type* str = new Type;
	context->parent->ns->members.insert({ this->name.text, str });

	str->type = Types::Struct;
	str->data = new Struct;
	str->data->name = this->name.text;
	str->data->expression = this;
	if (this->base_type.text.length())
		/*str->data->parent_struct =*/ context->parent->AdvanceTypeLookup(&str->data->parent_struct, this->base_type.text);
	if (this->templates)
	{
		str->data->templates.reserve(this->templates->size());
		for (auto ii : *this->templates)
		{
			str->data->templates.push_back({ 0/*context->parent->AdvanceTypeLookup(ii.first.text)*/, ii.second.text });
			context->parent->AdvanceTypeLookup(&str->data->templates.back().first, ii.first.text);
		}
	}

	//register the templates as a type, so all the members end up with the same type
	int size = 0;
	for (auto ii : this->members)
	{
		if (ii.type == StructMember::VariableMember)
			size++;
	}
	str->data->struct_members.reserve(size);
	for (auto ii : this->members)
	{
		if (ii.type == StructMember::VariableMember)
		{
			Type* type = 0;
			//if (this->templates == 0)
			//type = context->parent->AdvanceTypeLookup(ii.variable.first.text);

			str->data->struct_members.push_back({ ii.variable.second.text, ii.variable.first.text, type });
			if (this->templates == 0)
				context->parent->AdvanceTypeLookup(&str->data->struct_members.back().type, ii.variable.first.text);
		}
		else
		{
			if (this->templates == 0)
				ii.function->CompileDeclarations(context);
		}
	}

	if (this->templates == 0)
		this->AddConstructorDeclarations(str, context);
};

CValue DefaultExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&token);

	SwitchExpression* sw = dynamic_cast<SwitchExpression*>(this->Parent->Parent);
	if (sw == 0)
		context->parent->Error("Cannot use default expression outside of a switch!", token);

	//create a new block for this case
	llvm::BasicBlock* block = sw->def;

	//add the case to the switch
	bool is_first = sw->first_case;
	sw->first_case = false;

	//jump to end block
	if (!is_first)
		context->parent->builder.CreateBr(sw->switch_end);

	//start using our new block
	context->f->getBasicBlockList().push_back(block);
	context->parent->builder.SetInsertPoint(block);
	return CValue();
}

CValue CaseExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&token);

	SwitchExpression* sw = dynamic_cast<SwitchExpression*>(this->Parent->Parent);
	if (sw == 0)
		context->parent->Error("Cannot use case expression outside of a switch!", token);

	//create a new block for this case
	llvm::BasicBlock* block = llvm::BasicBlock::Create(llvm::getGlobalContext(), "case" + std::to_string(value));

	//add the case to the switch
	bool is_first = sw->AddCase(context->parent->builder.getInt32(this->value), block);

	//jump to end block
	if (!is_first)
		context->parent->builder.CreateBr(sw->switch_end);

	//start using our new block
	context->f->getBasicBlockList().push_back(block);
	context->parent->builder.SetInsertPoint(block);
	return CValue();
}

CValue SwitchExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&token);

	CValue value = this->var->Compile(context);
	if (value.type->type != Types::Int)
		context->parent->Error("Argument to Case Statement Must Be an Integer", token);

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
				context->parent->Error("Multiple defaults defined for the same switch!", token);
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
	this->sw = context->parent->builder.CreateSwitch(value.val, def, cases.size());

	//compile the block
	this->block->Compile(context);

	context->parent->builder.CreateBr(this->switch_end);

	if (no_def)
	{
		//insert and create a dummy default
		context->f->getBasicBlockList().push_back(def);
		context->parent->builder.SetInsertPoint(def);
		context->parent->builder.CreateBr(this->switch_end);
	}

	//start using end
	context->f->getBasicBlockList().push_back(this->switch_end);
	context->parent->builder.SetInsertPoint(this->switch_end);

	return CValue();
}