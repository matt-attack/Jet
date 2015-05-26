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

	auto rhs = right->Compile(context);

	return context->UnaryOperation(this->_operator.type, rhs);

	/*switch (this->_operator.type)
	{
	case TokenType::BNot:
	case TokenType::Minus:
	{
		if (dynamic_cast<BlockExpression*>(this->Parent))
			context->Pop();
		break;
	}
	default://operators that also do a store, like ++ and --
	{
		auto location = dynamic_cast<IStorableExpression*>(this->right);
		if (location)
		{
			if (dynamic_cast<BlockExpression*>(this->Parent) == 0)
				context->Duplicate();

			location->CompileStore(context);
		}
		else if (dynamic_cast<BlockExpression*>(this->Parent) != 0)
			context->Pop();
	}
	}*/
}

CValue PostfixExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&this->_operator);

	auto lhs = left->Compile(context);

	context->UnaryOperation(this->_operator.type, lhs);

	return lhs;
}

CValue IndexExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&token);

	auto string = dynamic_cast<StringExpression*>(index);
	if (string)
	{
		auto loc = this->GetGEP(context);
		return CValue(loc.type, context->parent->builder.CreateLoad(loc.val));
	}
	else
	{
		throw 7;
	}

	return CValue();
}

CValue IndexExpression::GetGEP(CompilerContext* context)
{
	auto p = dynamic_cast<NameExpression*>(left);
	auto i = dynamic_cast<IndexExpression*>(left);

	auto string = dynamic_cast<StringExpression*>(index);
	if (string && (p || i))
	{
		CValue lhs;
		if (p)
			lhs = context->named_values[p->GetName()];
		else if (i)
			lhs = i->GetGEP(context);

		int index = 0;
		for (; index < lhs.type->data->members.size(); index++)
		{
			if (lhs.type->data->members[index].first == string->GetValue())
				break;
		}
		if (index >= lhs.type->data->members.size())
		{
			Error("Struct Member '" + string->GetValue() + "' of Struct '" + lhs.type->data->name + "' Not Found", this->token);
			throw 7;//not found;
		}

		std::vector<llvm::Value*> iindex = { context->parent->builder.getInt32(0), context->parent->builder.getInt32(index) };

		auto loc = context->parent->builder.CreateGEP(lhs.val, iindex, "index");
		return CValue(lhs.type->data->members[index].second, loc);
	}
	throw 7;
}


void IndexExpression::CompileStore(CompilerContext* context, CValue right)
{
	context->CurrentToken(&token);

	auto string = dynamic_cast<StringExpression*>(index);
	if (string)
	{
		auto loc = this->GetGEP(context);
		right = context->DoCast(loc.type, right);
		context->parent->builder.CreateStore(right.val, loc.val);
	}
	else
	{
		throw 7;
	}
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
	return context->Number(this->value);
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

	auto fun = context->parent->functions[dynamic_cast<NameExpression*>(left)->GetName()];

	fun->Load(context->parent);

	auto f = context->parent->module->getFunction(dynamic_cast<NameExpression*>(left)->GetName());

	if (args->size() != f->arg_size())
	{
		//todo: add better checks later
		Error("Mismatched function parameters in call", token);
	}

	//build arg list
	std::vector<llvm::Value*> argsv;
	int i = 0;
	for (auto ii : *this->args)
	{
		//try and cast to the correct type if we can
		argsv.push_back(context->DoCast(fun->argst[i++].first, ii->Compile(context)).val);
	}

	//get actual method signature, and then lets do conversions
	return CValue(fun->return_type, context->parent->builder.CreateCall(f, argsv, "calltmp"));
}

CValue NameExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&token);
	//add load variable instruction
	//todo make me detect if this is a local or not
	return context->Load(token.text);
}

CValue OperatorAssignExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&token);

	//try and cast right side to left
	auto lhs = this->left->Compile(context);
	auto rhs = this->right->Compile(context);
	rhs = context->DoCast(lhs.type, rhs);

	auto res = context->BinaryOperation(token.type, lhs, rhs);

	//insert store here
	if (auto storable = dynamic_cast<IStorableExpression*>(this->left))
		storable->CompileStore(context, res);

	return CValue();
}

CValue OperatorExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&this->_operator);

	/*if (this->_operator.type == TokenType::And)
	{
	std::string label = "_endand"+context->GetUUID();
	this->left->Compile(context);
	context->JumpFalsePeek(label.c_str());//jump to endand if false
	context->Pop();
	this->right->Compile(context);
	context->Label(label);//put endand label here
	return;
	}

	if (this->_operator.type == TokenType::Or)
	{
	std::string label = "_endor"+context->GetUUID();
	this->left->Compile(context);
	context->JumpTruePeek(label.c_str());//jump to endor if true
	context->Pop();
	this->right->Compile(context);
	context->Label(label);//put endor label here
	return;
	}*/

	auto lhs = this->left->Compile(context);
	auto rhs = this->right->Compile(context);
	rhs = context->DoCast(lhs.type, rhs);

	return context->BinaryOperation(this->_operator.type, lhs, rhs);
}

CValue FunctionExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&token);

	std::string fname;
	if (name)
		fname = static_cast<NameExpression*>(name)->GetName();
	else
		fname = "_lambda_id_";

	//build list of types of vars
	std::vector<std::pair<Type*, std::string>> argsv;
	for (auto ii : *this->args)
	{
		argsv.push_back({ context->parent->LookupType(ii.first), ii.second });
	}

	CompilerContext* function = context->AddFunction(fname, context->parent->LookupType(this->ret_type), argsv);// , this->varargs);
	//ok, kinda hacky
	//int start = context->out.size();

	//ok push locals, in opposite order
	/*for (unsigned int i = 0; i < this->args->size(); i++)
	{
	auto aname = static_cast<NameExpression*>((*this->args)[i]);
	function->RegisterLocal(aname->GetName());
	}
	if (this->varargs)
	function->RegisterLocal(this->varargs->GetName());*/

	//alloc args
	auto AI = function->f->arg_begin();
	for (unsigned Idx = 0, e = args->size(); Idx != e; ++Idx, ++AI) {
		// Create an alloca for this variable.
		auto aname = (*this->args)[Idx].second;

		llvm::IRBuilder<> TmpB(&function->f->getEntryBlock(), function->f->getEntryBlock().begin());
		auto Alloca = TmpB.CreateAlloca(GetType(argsv[Idx].first), 0, aname);
		// Store the initial value into the alloca.
		function->parent->builder.CreateStore(AI, Alloca);

		AI->setName(aname);

		// Add arguments to variable symbol table.
		function->named_values[aname] = CValue(argsv[Idx].first, Alloca);
	}

	block->Compile(function);

	//if last instruction was a return, dont insert another one
	/*if (block->statements.size() > 0)
	{
	if (dynamic_cast<ReturnExpression*>(block->statements.at(block->statements.size()-1)) == 0)
	{
	function->Null();//return nil
	function->Return();
	}
	}
	else
	{
	function->Null();//return nil
	function->Return();
	}

	context->FinalizeFunction(function);

	//only named functions need to be stored here
	if (name)
	context->Store(static_cast<NameExpression*>(name)->GetName());
	*/

	//todo add dummy return
	//vm will pop off locals when it removes the call stack
	return CValue();
}

void FunctionExpression::CompileDeclarations(CompilerContext* context)
{
	std::string fname;
	if (name)
		fname = static_cast<NameExpression*>(name)->GetName();
	else
		fname = "_lambda_id_";

	Function* fun = new Function;
	fun->return_type = context->parent->AdvanceTypeLookup(this->ret_type);

	//std::vector<llvm::Type*> argsv;
	for (auto ii : *this->args)
	{
		auto type = context->parent->AdvanceTypeLookup(ii.first);

		fun->argst.push_back({ type, ii.second });
		//fun->args.push_back(GetType(type));
	}

	fun->name = fname;

	//CompilerContext* function = context->AddFunction(fname, this->args->size());// , this->varargs);
	//std::vector<llvm::Type*> Doubles(args->size(), llvm::Type::getDoubleTy(context->parent->context));
	//llvm::FunctionType *ft = llvm::FunctionType::get(GetType(fun->return_type), fun->args, false);
	//llvm::Function *f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, fname, context->parent->module);

	fun->f = 0;// f;
	context->parent->functions[fname] = fun;
}

CValue ExternExpression::Compile(CompilerContext* context)
{ //finish assign expression
	//context->Line(token.line);

	//std::string fname = static_cast<NameExpression*>(name)->GetName();

	//Function* fun = new Function;
	//fun->return_type = context->parent->LookupType(this->ret_type);

	////std::vector<llvm::Type*> argsv;
	//for (auto ii : *this->args)
	//{
	//	auto t = context->parent->LookupType(ii.first);
	//	fun->argst.push_back(t);
	//	fun->args.push_back(GetType(t));
	//}

	////CompilerContext* function = context->AddFunction(fname, this->args->size());// , this->varargs);
	////std::vector<llvm::Type*> Doubles(args->size(), llvm::Type::getDoubleTy(context->parent->context));
	//llvm::FunctionType *ft = llvm::FunctionType::get(GetType(fun->return_type), fun->args, false);
	//llvm::Function *f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, fname, context->parent->module);

	//fun->f = f;
	//context->parent->functions[fname] = fun;
	////ok, kinda hacky
	////int start = context->out.size();

	////ok push locals, in opposite order
	///*for (unsigned int i = 0; i < this->args->size(); i++)
	//{
	//auto aname = static_cast<NameExpression*>((*this->args)[i]);
	//function->RegisterLocal(aname->GetName());
	//}
	//if (this->varargs)
	//function->RegisterLocal(this->varargs->GetName());*/

	////alloc args
	//auto AI = f->arg_begin();
	//for (unsigned Idx = 0, e = args->size(); Idx != e; ++Idx, ++AI) {
	//	// Create an alloca for this variable.
	//	//llvm::AllocaInst *Alloca = CreateEntryBlockAlloca(F, Args[Idx]);
	//	auto aname = (*this->args)[Idx].second;

	//	//llvm::IRBuilder<> TmpB(&function->f->getEntryBlock(), function->f->getEntryBlock().begin());
	//	//auto Alloca = TmpB.CreateAlloca(llvm::Type::getDoubleTy(function->parent->context), 0, aname);
	//	// Store the initial value into the alloca.
	//	//function->parent->builder.CreateStore(AI, Alloca);

	//	AI->setName(aname);

	//	// Add arguments to variable symbol table.
	//	//function->named_values[aname] = Alloca;
	//}

	//block->Compile(function);

	//if last instruction was a return, dont insert another one
	/*if (block->statements.size() > 0)
	{
	if (dynamic_cast<ReturnExpression*>(block->statements.at(block->statements.size()-1)) == 0)
	{
	function->Null();//return nil
	function->Return();
	}
	}
	else
	{
	function->Null();//return nil
	function->Return();
	}

	context->FinalizeFunction(function);

	//only named functions need to be stored here
	if (name)
	context->Store(static_cast<NameExpression*>(name)->GetName());
	*/
	//vm will pop off locals when it removes the call stack
	return CValue();
}

void ExternExpression::CompileDeclarations(CompilerContext* context)
{
	std::string fname = static_cast<NameExpression*>(name)->GetName();

	Function* fun = new Function;
	fun->return_type = context->parent->AdvanceTypeLookup(this->ret_type);

	for (auto ii : *this->args)
	{
		auto type = context->parent->AdvanceTypeLookup(ii.first);

		fun->argst.push_back({ type, ii.second });
	}

	fun->name = fname;
	fun->f = 0;
	context->parent->functions[fname] = fun;
}

CValue LocalExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&(*_names)[0].second);

	int i = 0;
	for (auto ii : *this->_names) {
		auto aname = ii.second.text;

		llvm::IRBuilder<> TmpB(&context->f->getEntryBlock(), context->f->getEntryBlock().begin());

		Type* type = context->parent->LookupType(ii.first);
		auto Alloca = TmpB.CreateAlloca(GetType(type), 0, aname);
		// Store the initial value into the alloca.
		if (this->_right)
		{
			auto val = (*this->_right)[i++]->Compile(context);
			//cast it
			val = context->DoCast(type, val);
			context->parent->builder.CreateStore(val.val, Alloca);
		}

		// Add arguments to variable symbol table.
		context->named_values[aname] = CValue(type, Alloca);
	}

	return CValue();
}

CValue StructExpression::Compile(CompilerContext* context)
{
	//context->Line(token.line);

	return CValue();
}

void StructExpression::CompileDeclarations(CompilerContext* context)
{
	//build data about the struct
	//get or add the struct type from the type table
	Type* str = 0;
	auto ii = context->parent->types.find(this->name);
	if (ii == context->parent->types.end())//its new
	{
		str = new Type;
		context->parent->types[this->name] = str;
	}
	else
	{
		str = ii->second;
		if (str->type != Types::Invalid)
		{
			printf("Struct %s Already Defined!\n", this->name.c_str());
			throw 7;
		}
	}

	str->type = Types::Class;
	str->data = new Struct;
	str->data->name = this->name;
	for (auto ii : *this->elements)
	{
		auto type = context->parent->AdvanceTypeLookup(ii.first);

		str->data->members.push_back({ ii.second, type });
	}
};

