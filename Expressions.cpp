#include "Expressions.h"
#include "Compiler.h"
#include "Parser.h"

using namespace Jet;

#include <llvm\IR\IRBuilder.h>
#include <llvm\IR\Module.h>
#include <llvm\IR\LLVMContext.h>

/*void PrefixExpression::Compile(CompilerContext* context)
{
context->Line(this->_operator.line);

right->Compile(context);

context->UnaryOperation(this->_operator.type);

switch (this->_operator.type)
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
}
}*/

/*void PostfixExpression::Compile(CompilerContext* context)
{
context->Line(this->_operator.line);

left->Compile(context);

if (dynamic_cast<BlockExpression*>(this->Parent) == 0 && dynamic_cast<IStorableExpression*>(this->left))
context->Duplicate();

context->UnaryOperation(this->_operator.type);

if (dynamic_cast<IStorableExpression*>(this->left))
dynamic_cast<IStorableExpression*>(this->left)->CompileStore(context);
else if (dynamic_cast<BlockExpression*>(this->Parent) != 0)
context->Pop();
}*/

CValue IndexExpression::Compile(CompilerContext* context)
{
	context->Line(token.line);

	//ok, once again we need a pointer to the LHS...
	//so hack time
	auto p = dynamic_cast<NameExpression*>(left);
	auto lhs = context->named_values[p->GetName()];
	//auto lhs = left->Compile(context);

	//if the index is constant compile to a special instruction carying that constant
	if (auto string = dynamic_cast<StringExpression*>(index))
	{
		//treat it as a pointer to a struct atm
		int index = 0;
		for (; index < lhs.type->data->members.size(); index++)
		{
			if (lhs.type->data->members[index].first == string->GetValue())
				break;
		}
		if (index >= lhs.type->data->members.size())
			throw 7;//not found;

		//replace second zero with the index for the part we want
		//context->LoadIndex(string->GetValue().c_str());
		std::vector<llvm::Value*> iindex = { context->parent->builder.getInt32(0), context->parent->builder.getInt32(index) };
		//todo, embed type info
		auto ptr = context->parent->builder.CreateGEP(lhs.val, iindex, "index");

		//need to dereference
		return CValue(lhs.type->data->members[index].second, context->parent->builder.CreateLoad(ptr));
	}
	else
	{
		throw 7;
		//todo:
		auto idx = index->Compile(context);
		//context->LoadIndex();
	}

	return CValue();
}

void IndexExpression::CompileStore(CompilerContext* context, CValue right)
{
	//um, HACK
	auto p = dynamic_cast<NameExpression*>(left);
	auto lhs = context->named_values[p->GetName()];
	//auto lhs = left->Compile(context);

	if (auto string = dynamic_cast<StringExpression*>(index))
	{
		int index = 0;
		for (; index < lhs.type->data->members.size(); index++)
		{
			if (lhs.type->data->members[index].first == string->GetValue())
				break;
		}
		if (index >= lhs.type->data->members.size())
			throw 7;//not found;

		std::vector<llvm::Value*> iindex = { context->parent->builder.getInt32(0), context->parent->builder.getInt32(index) };
		//std::vector<llvm::Constant*> iindex = { const_inst32, const_inst32 };

		auto loc = context->parent->builder.CreateGEP(lhs.val, iindex, "index");
		context->parent->builder.CreateStore(right.val, loc);
	}
	else
	{
		throw 7;
	}
	/*context->Line(token.line);

	left->Compile(context);
	//if the index is constant compile to a special instruction carying that constant
	if (auto string = dynamic_cast<StringExpression*>(index))
	{
	context->StoreIndex(string->GetValue().c_str());
	}
	else
	{
	index->Compile(context);
	context->StoreIndex();
	}*/

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

	//pop off if we dont need the result
	//if (dynamic_cast<BlockExpression*>(this->Parent))
	//context->Pop();
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
	//context->Store(this->right->Compile(context));
	//this->right->Compile(context);

	//context->Store()

	//if (dynamic_cast<BlockExpression*>(this->Parent) == 0)
	//	context->Duplicate();//if my parent is not block expression, we need the result, so push it

	if (auto storable = dynamic_cast<IStorableExpression*>(this->left))
		storable->CompileStore(context, right->Compile(context));
	return CValue();
}

CValue CallExpression::Compile(CompilerContext* context)
{
	auto fun = context->parent->functions[dynamic_cast<NameExpression*>(left)->GetName()];

	fun->Load(context->parent);

	auto f = context->parent->module->getFunction(dynamic_cast<NameExpression*>(left)->GetName());


	if (args->size() != f->arg_size())
		throw 7;

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

	/*context->Line(token.line);

	//need to check if left is a local, or a captured value before looking at globals
	if (dynamic_cast<NameExpression*>(left) && context->IsLocal(dynamic_cast<NameExpression*>(left)->GetName()) == false)
	{
	//push args onto stack
	for (auto i: *args)
	i->Compile(context);

	context->Call(dynamic_cast<NameExpression*>(left)->GetName(), args->size());
	}
	else// if (dynamic_cast<IStorableExpression*>(left) != 0)
	{
	auto index = dynamic_cast<IndexExpression*>(left);
	if (index && index->token.type == TokenType::Colon)//its a "self" call
	{
	index->left->Compile(context);//push object as the first argument

	//push args onto stack
	for (auto i: *args)
	i->Compile(context);//pushes args

	//compile left I guess?
	left->Compile(context);//pushes function

	//increase number of args
	context->ECall(args->size()+1);
	}
	else
	{
	//push args onto stack
	for (auto i: *args)
	i->Compile(context);

	//compile left I guess?
	left->Compile(context);

	context->ECall(args->size());
	}
	}
	//else
	//{
	//throw ParserException(token.filename, token.line, "Error: Cannot call an expression that is not a name");
	//}
	//help, how should I handle this for multiple returns
	//pop off return value if we dont need it
	if (dynamic_cast<BlockExpression*>(this->Parent))
	context->Pop();*///if my parent is block expression, we dont the result, so pop it
	return CValue();
}

CValue NameExpression::Compile(CompilerContext* context)
{
	//add load variable instruction
	//todo make me detect if this is a local or not
	return context->Load(name);

	//if (dynamic_cast<BlockExpression*>(this->Parent))
	//context->Pop();
}

CValue OperatorAssignExpression::Compile(CompilerContext* context)
{
	context->Line(token.line);

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
	context->Line(this->_operator.line);

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
	context->Line(token.line);

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
		//llvm::AllocaInst *Alloca = CreateEntryBlockAlloca(F, Args[Idx]);
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
	context->Line(token.line);

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
	/*auto AI = f->arg_begin();
	for (unsigned Idx = 0, e = args->size(); Idx != e; ++Idx, ++AI) {
	// Create an alloca for this variable.
	//llvm::AllocaInst *Alloca = CreateEntryBlockAlloca(F, Args[Idx]);
	auto aname = (*this->args)[Idx].second;

	//llvm::IRBuilder<> TmpB(&function->f->getEntryBlock(), function->f->getEntryBlock().begin());
	//auto Alloca = TmpB.CreateAlloca(llvm::Type::getDoubleTy(function->parent->context), 0, aname);
	// Store the initial value into the alloca.
	//function->parent->builder.CreateStore(AI, Alloca);

	AI->setName(aname);

	// Add arguments to variable symbol table.
	//function->named_values[aname] = Alloca;
	}*/
}

CValue LocalExpression::Compile(CompilerContext* context)
{
	context->Line((*_names)[0].second.line);

	int i = 0;

	for (auto ii : *this->_names) {
		// Create an alloca for this variable.
		//llvm::AllocaInst *Alloca = CreateEntryBlockAlloca(F, Args[Idx]);
		auto aname = ii.second.getText();// static_cast<NameExpression*>((*this->args)[Idx])->GetName();

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
		//use the type info
		context->named_values[aname] = CValue(type, Alloca);
	}

	return CValue();
}

CValue StructExpression::Compile(CompilerContext* context)
{
	context->Line(token.line);

	/*Struct* s = new Struct;
	std::vector<llvm::Type*> elementss;
	for (auto ii : *this->elements)
	{
	auto type = context->parent->LookupType(ii.first);
	//s->members.push_back({ ii.second, type });
	elementss.push_back(GetType(type));
	}
	auto type = llvm::StructType::create(elementss, this->name);
	//context->parent->module->getOrInsertGlobal("testing", type);
	//add me to the list!
	s->type = type;

	context->parent->types[this->name] = Type(Types::Class, s);*/
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

