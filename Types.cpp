#include "Types.h"
#include "Compiler.h"
#include "CompilerContext.h"
#include "Expressions.h"

using namespace Jet;

Type Jet::VoidType("void", Types::Void);
Type Jet::BoolType("bool", Types::Bool);
Type Jet::DoubleType("double", Types::Double);
Type Jet::IntType("int", Types::Int);

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
	case Types::Struct:
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

	if (type == Types::Struct)
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

Type* Type::Instantiate(Compiler* compiler, const std::vector<Type*>& types)
{
	//register the types
	int i = 0;
	for (auto ii : this->data->templates)
	{
		//lets be stupid and just register the type
		//CHANGE ME LATER, THIS OVERRIDES TYPES, OR JUST RESTORE AFTER THIS
		compiler->types[ii.second] = types[i++];
	}
	//printf("tried to instantiate template");

	//duplicate and load
	Struct* str = new Struct;
	//str->functions = this->data->functions;
	//str->members = this->data->members;
	//build members
	for (auto ii : *this->data->expression->elements)
	{
		auto type = compiler->AdvanceTypeLookup(ii.first);

		str->members.push_back({ ii.second, ii.first, type });
	}

	str->template_base = this->data;
	str->name = this->data->name + "<";
	for (int i = 0; i < this->data->templates.size(); i++)
	{
		str->name += types[i]->ToString();
		if (i < this->data->templates.size()-1)
			str->name += ',';
	}
	str->name += ">";
	str->expression = this->data->expression;

	Type* t = new Type(str->name, Types::Struct, str);
	t->Load(compiler);

	return t;
}

std::string Type::ToString()
{
	switch (type)
	{
	case Types::Struct:
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
	case Types::Invalid:
		return "Undefined Type";
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
		auto type = ii.type;
		ii.type->Load(compiler);

		elementss.push_back(GetType(type));
	}
	this->type = llvm::StructType::create(elementss, this->name);
	//this->type->dump();
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