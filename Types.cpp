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
		if (this->name.back() == '>')
		{
			//im a template!
			//get first bit, then we can instatiate it
			int p = 0;
			for (p = 0; p < name.length(); p++)
				if (name[p] == '<')
					break;

			std::string base = name.substr(0, p);

			//parse types
			std::vector<Type*> types;
			p++;
			do
			{
				//lets cheat for the moment ok
				std::string subtype;
				do
				{
					subtype += name[p];
					p++;
				} while (name[p] != ',' && name[p] != '>');

				Type* t = compiler->LookupType(subtype);
				types.push_back(t);
			} while (name[p++] != '>');

			//look up the base, and lets instantiate it
			auto t = compiler->types.find(base);
			if (t == compiler->types.end())
				Error("Reference To Undefined Type '" + base + "'", *compiler->current_function->current_token);

			Type* res = t->second->Instantiate(compiler, types);
			*this = *res;
			//compiler->types[name] = res;

			//compile its functions
			if (res->data->expression->members.size() > 0)
			{
				StructExpression* expr = dynamic_cast<StructExpression*>(res->data->expression);// ->functions->back()->Parent);
				auto oldname = expr->name;
				expr->name.text = res->data->name;

				//store then restore insertion point
				auto rp = compiler->builder.GetInsertBlock();

				for (auto ii : res->data->expression->members)//functions)
					if (ii.type == StructMember::FunctionMember)
						ii.function->CompileDeclarations(compiler->current_function);

				for (auto ii : res->data->expression->members)//functions)
					if (ii.type == StructMember::FunctionMember)
						ii.function->Compile(compiler->current_function);//the context used may not be proper, but it works

				compiler->builder.SetInsertPoint(rp);
				expr->name = oldname;
			}
			//Error("Not implemented", *compiler->current_function->current_token);
		}
		else
			Error("Tried To Use Undefined Type '" + this->name + "'", *compiler->current_function->current_token);
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

	//duplicate and load
	Struct* str = new Struct;
	//str->functions = this->data->functions;
	//str->members = this->data->members;
	//build members
	for (auto ii : this->data->expression->members)
	{
		if (ii.type == StructMember::VariableMember)
		{
			auto type = compiler->AdvanceTypeLookup(ii.variable.first.text);

			str->members.push_back({ ii.variable.second.text, ii.variable.first.text, type });
		}
	}

	str->template_base = this->data;
	str->name = this->data->name + "<";
	for (int i = 0; i < this->data->templates.size(); i++)
	{
		str->name += types[i]->ToString();
		if (i < this->data->templates.size() - 1)
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

	this->loaded = true;
}

//#include <llvm/IR/Attributes.h>
//#include <llvm/IR/Argument.h>
//#include <llvm/ADT/ilist.h>
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

	/*if (this->name[0] == 'R')
	{
	//f->setDLLStorageClass(llvm::Function::DLLImportStorageClass);
	//f->setCallingConv(llvm::CallingConv::X86_StdCall);

	int i = 0;
	for (auto ii : this->argst)
	{
	if (ii.first->type == Types::Struct)
	{

	auto list = f->args();

	//for (auto arg = list.begin(); arg != list.end(); ++arg)
	//{
	//	arg->dump();
	//	llvm::AttributeSet set;
	//	//set.
	//	llvm::AttrBuilder builder;

	//	builder.addAttribute(llvm::Attribute::AttrKind::ByVal);
	//	set = set.addAttribute(compiler->context, 0, llvm::Attribute::AttrKind::ByVal);
	//	//set.addAttribute()
	//	arg->addAttr(set);
	//	printf("hi");
	//}

	}
	i++;
	}
	}*/
	//alloc args
	auto AI = f->arg_begin();
	for (unsigned Idx = 0, e = argst.size(); Idx != e; ++Idx, ++AI)
	{
		auto aname = this->argst[Idx].second;

		AI->setName(aname);
	}

	this->loaded = true;
}