#include "Types.h"
#include "Compiler.h"

using namespace Jet;

Type Jet::VoidType(Types::Void);
Type Jet::BoolType(Types::Bool);
Type Jet::DoubleType(Types::Double);
Type Jet::IntType(Types::Int);

llvm::Type* Jet::GetType(Type* t)
{
	switch (t->type)
	{
	case Types::Double:
		return llvm::Type::getDoubleTy(llvm::getGlobalContext());
	case Types::Float:
		return llvm::Type::getFloatTy(llvm::getGlobalContext());
	case Types::Int:
		return llvm::Type::getInt32Ty(llvm::getGlobalContext());
	case Types::Void:
		return llvm::Type::getVoidTy(llvm::getGlobalContext());
	case Types::Char:
		return llvm::Type::getInt8Ty(llvm::getGlobalContext());
	case Types::Short:
		return llvm::Type::getInt16Ty(llvm::getGlobalContext());
	case Types::Bool:
		return llvm::Type::getInt1Ty(llvm::getGlobalContext());
	case Types::Class:
		return t->data->type;
	case Types::Array:
		return llvm::ArrayType::get(GetType(t->base), t->size);
	case Types::Pointer:
		return llvm::PointerType::get(GetType(t->base), 0);//address space, wat?
	}
	throw 7;
}

void Type::Load(Compiler* compiler)
{
	//recursively load
	if (loaded == true)
		return;

	if (type == Types::Class)
	{
		data->Load(compiler);
	}
	else if (type == Types::Invalid)
	{
		//get a good error here!!!
		Error("Tried To Use Undefined Type", *compiler->current_function->current_token);
		//printf("Tried to use undefined type\n");
		//throw 7;
	}
	else if (type == Types::Pointer)
	{
		//load recursively
		this->base->Load(compiler);
	}
	this->loaded = true;
}

std::string Type::ToString()
{
	switch (type)
	{
	case Types::Class:
		return this->data->name;
	case Types::Pointer:
		return this->base->ToString() + "*";
	case Types::Array:
		return this->base->ToString() + "[" + std::to_string(this->size) + "]";
	case Types::Bool:
		return "bool";
	case Types::Char:
		return "char";
	case Types::Int:
		return "int";
	case Types::Float:
		return "float";
	case Types::Double:
		return "double";
	case Types::Short:
		return "short";
	case Types::Void:
		return "void";
	}
}

void Struct::Load(Compiler* compiler)
{
	if (this->loaded)
		return;

	//recursively load
	std::vector<llvm::Type*> elementss;
	for (auto ii : this->members)
	{
		auto type = ii.second;
		ii.second->Load(compiler);

		elementss.push_back(GetType(type));
	}
	this->type = llvm::StructType::create(elementss, this->name);

	this->loaded = true;
}

void Function::Load(Compiler* compiler)
{
	if (this->loaded)
		return;

	this->return_type->Load(compiler);

	for (auto type : this->argst)
	{
		type.first->Load(compiler);
		this->args.push_back(GetType(type.first));
	}

	llvm::FunctionType *ft = llvm::FunctionType::get(GetType(this->return_type), this->args, false);
	this->f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, name, compiler->module);

	//alloc args
	auto AI = f->arg_begin();
	for (unsigned Idx = 0, e = argst.size(); Idx != e; ++Idx, ++AI)
	{
		auto aname = this->argst[Idx].second;

		AI->setName(aname);
	}

	this->loaded = true;
}