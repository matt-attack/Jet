#include "Expressions.h"
#include "Compiler.h"
#include "Parser.h"

using namespace Jet;

#include <llvm\IR\IRBuilder.h>
#include <llvm\IR\Module.h>
#include <llvm\IR\LLVMContext.h>

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
			return CValue(context->parent->LookupType(var.type->ToString() + "*"), var.val);
		}
		else if (p)
		{
			auto var = p->GetGEP(context);
			return CValue(context->parent->LookupType(var.type->ToString() + "*"), var.val);
		}
		Error("Not Implemented", this->_operator);
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

	auto loc = this->GetGEP(context);
	return CValue(loc.type, context->parent->builder.CreateLoad(loc.val));
}

Type* IndexExpression::GetBaseType(CompilerContext* context)
{
	auto p = dynamic_cast<NameExpression*>(left);
	auto i = dynamic_cast<IndexExpression*>(left);

	if (p)
		return context->GetVariable(p->GetName()).type;
	else if (i)
		return i->GetType(context);

	Error("wat", token);
}

Type* IndexExpression::GetBaseType(Compiler* compiler)
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
		Error("todo", token);// throw 7;// return i->GetType(context);

	Error("wat", token);
}

CValue IndexExpression::GetBaseGEP(CompilerContext* context)
{
	auto p = dynamic_cast<NameExpression*>(left);
	auto i = dynamic_cast<IndexExpression*>(left);

	if (p)
		return context->GetVariable(p->GetName());
	else if (i)
		return i->GetGEP(context);

	Error("wat", token);
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
			lhs = i->GetGEP(context);

		if (string && lhs.type->type == Types::Struct)
		{
			int index = 0;
			for (; index < lhs.type->data->members.size(); index++)
			{
				if (lhs.type->data->members[index].name == string->GetValue())
					break;
			}

			if (index >= lhs.type->data->members.size())
			{
				Error("Struct Member '" + string->GetValue() + "' of Struct '" + lhs.type->data->name + "' Not Found", this->token);
				//not found;
			}

			return lhs.type->data->members[index].type;
		}
		else if (lhs.type->type == Types::Array && string == 0)//or pointer!!(later)
		{
			return lhs.type->base;
		}
	}
}

CValue IndexExpression::GetGEP(CompilerContext* context)
{
	auto p = dynamic_cast<NameExpression*>(left);
	auto i = dynamic_cast<IndexExpression*>(left);

	//auto string = dynamic_cast<StringExpression*>(index);
	if (index == 0 && this->token.type == TokenType::Pointy)
	{
		CValue lhs;
		if (p)
			lhs = context->GetVariable(p->GetName());
		else if (i)
			lhs = i->GetGEP(context);

		if (lhs.type->type == Types::Pointer && lhs.type->base->type == Types::Struct)
		{
			lhs.val = context->parent->builder.CreateLoad(lhs.val);

			auto type = lhs.type->base;
			int index = 0;
			for (; index < type->data->members.size(); index++)
			{
				if (type->data->members[index].name == this->member.text)
					break;
			}
			if (index >= type->data->members.size())
			{
				Error("Struct Member '" + this->member.text + "' of Struct '" + type->data->name + "' Not Found", this->token);
			}

			std::vector<llvm::Value*> iindex = { context->parent->builder.getInt32(0), context->parent->builder.getInt32(index) };

			auto loc = context->parent->builder.CreateGEP(lhs.val, iindex, "index");
			return CValue(type->data->members[index].type, loc);
		}

		Error("unimplemented!", this->token);
	}
	else if (p || i)
	{
		CValue lhs;
		if (p)
			lhs = context->GetVariable(p->GetName());
		else if (i)
			lhs = i->GetGEP(context);

		if (this->member.text.length() && lhs.type->type == Types::Struct)
		{
			int index = 0;
			for (; index < lhs.type->data->members.size(); index++)
			{
				if (lhs.type->data->members[index].name == this->member.text)
					break;
			}
			if (index >= lhs.type->data->members.size())
			{
				Error("Struct Member '" + this->member.text + "' of Struct '" + lhs.type->data->name + "' Not Found", this->token);
				//not found;
			}

			std::vector<llvm::Value*> iindex = { context->parent->builder.getInt32(0), context->parent->builder.getInt32(index) };

			auto loc = context->parent->builder.CreateGEP(lhs.val, iindex, "index");
			return CValue(lhs.type->data->members[index].type, loc);
		}
		else if (lhs.type->type == Types::Array && this->member.text.length() == 0)//or pointer!!(later)
		{
			std::vector<llvm::Value*> iindex = { context->parent->builder.getInt32(0), context->DoCast(&IntType, index->Compile(context)).val };

			auto loc = context->parent->builder.CreateGEP(lhs.val, iindex, "index");

			return CValue(lhs.type->base, loc);
		}
		Error("Cannot index type '" + lhs.type->ToString() + "'", this->token);
	}

	Error("Unimplemented", this->token);
}


void IndexExpression::CompileStore(CompilerContext* context, CValue right)
{
	context->CurrentToken(&token);

	auto loc = this->GetGEP(context);
	right = context->DoCast(loc.type, right);
	context->parent->builder.CreateStore(right.val, loc.val);
}

/*void ObjectExpression::Compile(CompilerContext* context)
{
int count = 0;
if (this->inits)
{
count = this->inits->size();
//set these up
for (auto ii: *this->inits)
{
context->String(ii.first);
ii.second->Compile(context);
}
}
context->NewObject(count);

//pop off if we dont need the result
if (dynamic_cast<BlockExpression*>(this->Parent))
context->Pop();
}

void ArrayExpression::Compile(CompilerContext* context)
{
int count = this->initializers.size();
for (auto i: this->initializers)
i->Compile(context);

context->NewArray(count);

//pop off if we dont need the result
if (dynamic_cast<BlockExpression*>(this->Parent))
context->Pop();
}*/

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
	for (int i = 0; i < this->token.text.length(); i++)
	{
		if (this->token.text[i] == '.')
			isint = false;
	}
	//ok, lets get the type from what kind of constant it is
	//get type from the constant
	//this is pretty terrible, come back later
	if (isint)
		return context->Integer(std::stoi(this->token.text));
	else
		return context->Float(this->value);
}

/*void SwapExpression::Compile(CompilerContext* context)
{
right->Compile(context);
left->Compile(context);

if (auto rstorable = dynamic_cast<IStorableExpression*>(this->right))
rstorable->CompileStore(context);

if (dynamic_cast<BlockExpression*>(this->Parent) == 0)
context->Duplicate();

if (auto lstorable = dynamic_cast<IStorableExpression*>(this->left))
lstorable->CompileStore(context);
}*/

CValue AssignExpression::Compile(CompilerContext* context)
{
	if (auto storable = dynamic_cast<IStorableExpression*>(this->left))
		storable->CompileStore(context, right->Compile(context));

	return CValue();
}

CValue CallExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&this->token);

	std::vector<CValue> argsv;

	std::string fname;
	Type* stru = 0;
	if (auto name = dynamic_cast<NameExpression*>(left))
	{
		//ok handle what to do if im an index expression
		fname = name->GetName();
	}
	else if (auto index = dynamic_cast<IndexExpression*>(left))
	{
		//im a struct yo
		fname = index->member.text;
		//if (index->member.length() > 0)
		//	fname = index->member;
		//else
		//	fname = dynamic_cast<StringExpression*>(index->index)->GetValue();
		stru = index->GetBaseType(context);

		llvm::Value* self;
		if (index->token.type == TokenType::Pointy)// && index->token.type != TokenType::Dot)//->member.length() > 0)
		{
			stru = stru->base;
			self = context->parent->builder.CreateLoad(index->GetBaseGEP(context).val);
		}
		else
			self = index->GetBaseGEP(context).val;

		//push in the this pointer argument kay
		argsv.push_back(CValue(context->parent->LookupType(stru->ToString() + "*"), self));
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

	auto res = context->BinaryOperation(token.type, lhs, rhs);

	//insert store here
	if (auto storable = dynamic_cast<IStorableExpression*>(this->left))
		storable->CompileStore(context, res);

	return CValue();
}

CValue OperatorExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&this->_operator);

	if (this->_operator.type == TokenType::And)
	{
		auto else_block = llvm::BasicBlock::Create(llvm::getGlobalContext(), "land.shortcircuitelse");
		auto end_block = llvm::BasicBlock::Create(llvm::getGlobalContext(), "land.endshortcircuit");
		auto cur_block = context->parent->builder.GetInsertBlock();

		auto cond = this->left->Compile(context);
		cond = context->DoCast(&BoolType, cond);
		context->parent->builder.CreateCondBr(cond.val, else_block, end_block);

		context->f->getBasicBlockList().push_back(else_block);
		context->parent->builder.SetInsertPoint(else_block);
		auto cond2 = this->right->Compile(context);

		cond2 = context->DoCast(&BoolType, cond2);
		context->parent->builder.CreateBr(end_block);

		context->f->getBasicBlockList().push_back(end_block);
		context->parent->builder.SetInsertPoint(end_block);
		auto phi = context->parent->builder.CreatePHI(GetType(cond.type), 2, "land");
		phi->addIncoming(cond.val, cur_block);
		phi->addIncoming(cond2.val, else_block);

		return CValue(&BoolType, phi);
	}

	if (this->_operator.type == TokenType::Or)
	{
		auto else_block = llvm::BasicBlock::Create(llvm::getGlobalContext(), "lor.shortcircuitelse");
		auto end_block = llvm::BasicBlock::Create(llvm::getGlobalContext(), "lor.endshortcircuit");
		auto cur_block = context->parent->builder.GetInsertBlock();

		auto cond = this->left->Compile(context);
		cond = context->DoCast(&BoolType, cond);
		context->parent->builder.CreateCondBr(cond.val, end_block, else_block);

		context->f->getBasicBlockList().push_back(else_block);
		context->parent->builder.SetInsertPoint(else_block);
		auto cond2 = this->right->Compile(context);
		cond2 = context->DoCast(&BoolType, cond2);
		context->parent->builder.CreateBr(end_block);

		context->f->getBasicBlockList().push_back(end_block);
		context->parent->builder.SetInsertPoint(end_block);

		auto phi = context->parent->builder.CreatePHI(GetType(cond.type), 2, "lor");
		phi->addIncoming(cond.val, cur_block);
		phi->addIncoming(cond2.val, else_block);
		return CValue(&BoolType, phi);
	}

	auto lhs = this->left->Compile(context);
	auto rhs = this->right->Compile(context);
	rhs = context->DoCast(lhs.type, rhs);

	return context->BinaryOperation(this->_operator.type, lhs, rhs);
}

std::string FunctionExpression::GetRealName()
{
	std::string fname;
	if (name.text.length() > 0)
		fname = name.text;
	else
		fname = "_lambda_id_";
	auto Struct = dynamic_cast<StructExpression*>(this->Parent);
	return Struct ? "__" + Struct->GetName() + "_" + fname : fname;
}

CValue FunctionExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&token);

	std::string fname;
	if (name.text.length() > 0)
		fname = name.text;
	else
		fname = "_lambda_id_";

	//build list of types of vars
	std::vector<std::pair<Type*, std::string>> argsv;
	auto Struct = dynamic_cast<StructExpression*>(this->Parent) ? dynamic_cast<StructExpression*>(this->Parent)->GetName() : this->Struct.text;
	if (Struct.length() > 0)
	{
		//im a member function
		//insert first arg, which is me
		auto type = context->parent->LookupType(Struct + "*");

		argsv.push_back({ type, "this" });
		//this->args->insert(this->args->begin(), { Struct + "*", "this" });
		//this->args->push_back({ Struct + "*", "this" });
	}
	for (auto ii : *this->args)
	{
		argsv.push_back({ context->parent->LookupType(ii.first), ii.second });
	}

	//auto Struct = dynamic_cast<StructExpression*>(this->Parent) ? dynamic_cast<StructExpression*>(this->Parent)->GetName() : this->Struct.text;
	CompilerContext* function = context->AddFunction(this->GetRealName(), context->parent->LookupType(this->ret_type.text), argsv, Struct.length() > 0 ? true : false);// , this->varargs);
	if (Struct.length() > 0)
	{
		auto range = context->parent->types[Struct]->data->functions.equal_range(fname);
		for (auto ii = range.first; ii != range.second; ii++)
		{
			//printf("found function option for %s\n", name.c_str());

			//pick one with the right number of args
			if (ii->second->argst.size() == argsv.size())
				ii->second->f = function->f;
		}
	}
	//else
	//context->parent->functions[fname] = function->f;

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

		// Add arguments to variable symbol table.
		function->RegisterLocal(aname, CValue(argsv[Idx].first, Alloca));
	}

	block->Compile(function);

	//check for return, and insert one or error if there isnt one
	if (function->f->getBasicBlockList().back().getTerminator() == 0)
		if (this->ret_type.text == "void")
			function->Return(CValue());
		else
		{
			//function->f->dump();
			Error("Function must return a value!", token);
		}

	return CValue();
}

void FunctionExpression::CompileDeclarations(CompilerContext* context)
{
	std::string fname;
	if (name.text.length() > 0)
		fname = name.text;
	else
		fname = "_lambda_id_";

	Function* fun = new Function;
	fun->return_type = context->parent->AdvanceTypeLookup(this->ret_type.text);
	fun->expression = this;

	auto Struct = dynamic_cast<StructExpression*>(this->Parent) ? dynamic_cast<StructExpression*>(this->Parent)->GetName() : this->Struct.text;
	if (Struct.length() > 0)
	{
		//im a member function
		//insert first arg, which is me
		auto type = context->parent->AdvanceTypeLookup(Struct + "*");

		fun->argst.push_back({ type, "this" });
		//this->args->insert(this->args->begin(), { Struct + "*", "this" });
		//this->args->push_back({ Struct + "*", "this" });
	}

	for (auto ii : *this->args)
	{
		auto type = context->parent->AdvanceTypeLookup(ii.first);

		fun->argst.push_back({ type, ii.second });
	}

	//todo: modulate actual name of function
	fun->name = this->GetRealName();
	fun->f = 0;
	if (Struct.length() > 0)
		context->parent->types[Struct]->data->functions.insert({ fname, fun });
	else
		context->parent->functions.insert({ fname, fun });
}

CValue ExternExpression::Compile(CompilerContext* context)
{
	return CValue();
}

void ExternExpression::CompileDeclarations(CompilerContext* context)
{
	std::string fname = name.text;

	Function* fun = new Function;
	fun->return_type = context->parent->AdvanceTypeLookup(this->ret_type.text);

	for (auto ii : *this->args)
	{
		auto type = context->parent->AdvanceTypeLookup(ii.first);

		fun->argst.push_back({ type, ii.second });
	}

	fun->name = fname;

	fun->f = 0;
	if (Struct.length() > 0)
	{
		fun->name = "__" + Struct + "_" + fname;//mangled name

		
		//add to struct
		auto ii = context->parent->types.find(Struct);
		if (ii == context->parent->types.end())//its new
		{
			Error("Not implemented!", token);
			//str = new Type;
			//context->parent->types[this->name] = str;
		}
		else
		{
			if (ii->second->type != Types::Struct)
				Error("Cannot define a function for a type that is not a struct", token);

			ii->second->data->functions.insert({ fname, fun });
		}
	}
	else
	{
		context->parent->functions.insert({ fname, fun });// [fname] = fun;
	}
}

CValue LocalExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&(*_names)[0].second);

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
				Alloca = TmpB.CreateAlloca(GetType(type), 0, aname);

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

			context->parent->builder.CreateStore(val.val, Alloca);
		}
		else
			Error("Cannot infer type from nothing!", ii.second);

		// Add arguments to variable symbol table.
		context->RegisterLocal(aname, CValue(type, Alloca));

		//construct it!
		if (this->_right == 0 && type->type == Types::Struct)
		{
			//call default construct if it exists
			const std::string& constructor_name = type->data->template_base ? type->data->template_base->name : type->data->name;
			auto iter = type->data->functions.find(constructor_name);
			if (iter != type->data->functions.end())
			{
				//this is the constructor, call it

				//todo: move implicit casts for operators and assignment into functions in compilercontext
				//	will make it easier to implement operator overloads
				context->Call(constructor_name, { CValue(context->parent->LookupType(type->ToString() + "*"), Alloca) }, type);
			}
		}
	}

	return CValue();
}

CValue StructExpression::Compile(CompilerContext* context)
{
	if (this->templates)
	{
		//compile l8r
		return CValue();
	}

	for (auto ii : this->members)
	{
		if (ii.type == StructMember::FunctionMember)
			ii.function->Compile(context);
	}
	return CValue();
}

void StructExpression::CompileDeclarations(CompilerContext* context)
{
	//build data about the struct
	//get or add the struct type from the type table
	Type* str = 0;
	auto ii = context->parent->types.find(this->name.text);
	if (ii == context->parent->types.end())//its new
	{
		str = new Type;
		context->parent->types[this->name.text] = str;
	}
	else
	{
		str = ii->second;
		if (str->type != Types::Invalid)
		{
			Error("Struct '" + this->name.text + "' Already Defined", this->token);
		}
	}

	str->type = Types::Struct;
	str->data = new Struct;
	str->data->name = this->name.text;
	str->data->expression = this;
	if (this->templates)
	{
		for (auto ii : *this->templates)
		{
			str->data->templates.push_back({ ii.first.text, ii.second.text });
		}
	}
	//str->data->templates = this->templates;

	for (auto ii : this->members)
	{
		if (ii.type == StructMember::VariableMember)
		{
			Type* type = 0;
			if (this->templates == 0)
				type = context->parent->AdvanceTypeLookup(ii.variable.first.text);

			str->data->members.push_back({ ii.variable.second.text, ii.variable.first.text, type });
		}
		else
		{
			ii.function->CompileDeclarations(context);
		}
	}

	//if (this->templates == 0)
	//{
	/*for (auto ii : *this->functions)
	{
		ii->CompileDeclarations(context);
	}*/
	//}
};

CValue DefaultExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&token);

	SwitchExpression* sw = dynamic_cast<SwitchExpression*>(this->Parent->Parent);
	if (sw == 0)
		Error("Cannot use default expression outside of a switch!", token);

	//create a new block for this case
	llvm::BasicBlock* block = sw->def;// lvm::BasicBlock::Create(llvm::getGlobalContext(), "switchdefault");

	//add the case to the switch
	bool is_first = sw->first_case;// AddCase(context->parent->builder.getInt32(this->value), block);
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
		Error("Cannot use case expression outside of a switch!", token);

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
		Error("Argument to Case Statement Must Be an Integer", token);

	this->switch_end = llvm::BasicBlock::Create(llvm::getGlobalContext(), "switchend");

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
				Error("Multiple defaults defined for the same switch!", token);
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