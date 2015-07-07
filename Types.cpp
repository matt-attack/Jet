#include "Types.h"
#include "Compiler.h"
#include "CompilerContext.h"
#include "Expressions.h"

//#include <llvm/IR/Attributes.h>
//#include <llvm/IR/Argument.h>
//#include <llvm/ADT/ilist.h>

using namespace Jet;

Type Jet::VoidType("void", Types::Void);

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
	case Types::Long:
		return llvm::Type::getInt64Ty(llvm::getGlobalContext());
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

			//look up the base, and lets instantiate it
			std::string base = name.substr(0, p);
			auto t = compiler->types.find(base);
			if (t == compiler->types.end())
				Error("Reference To Undefined Type '" + base + "'", *compiler->current_function->current_token);
			else if (t->second->type == Types::Trait)
			{
				printf("was a trait");
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

					Type* t = compiler->AdvanceTypeLookup(subtype);
					types.push_back(t);
				} while (name[p++] != '>');

				this->type = Types::Trait;
				this->trait = new Trait;
				*this->trait = *t->second->trait;
				this->trait->template_args = types;

				return;
			}

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
			
			Type* res = t->second->Instantiate(compiler, types);
			//check to see if it was already instantiated
			auto realname = res->ToString();
			auto iter = compiler->types.find(realname);
			if (iter == compiler->types.end())
			{
				*this = *res;

				//make sure the real thing is stored as this
				auto realname = res->ToString();

				compiler->types[realname] = res;
			}
			else
			{
				if (this != iter->second)
					*this = *iter->second;
				else
					*this = *res;
			}	

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
						ii.function->DoCompile(compiler->current_function);//the context used may not be proper, but it works

				compiler->builder.SetInsertPoint(rp);
				expr->name = oldname;
			}
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


//is is function, b is trait fruncton
bool IsMatch(Function* a, Function* b)
{
	if (a->return_type != b->return_type)
		return false;

	if (a->argst.size() != b->argst.size() - 1)
		return false;

	for (int i = 1; i < a->argst.size(); i++)
		if (a->argst[i].first != b->argst[i - 1].first)
			return false;

	return true;
}

void FindTemplates(Compiler* compiler, Type** types, Type* type, Type* match_type, Trait* trait)
{
	int i = 0;
	bool was_template = false;
	for (auto temp : trait->templates)
	{
		if (match_type->name == temp.second)
		{
			if (types[i] && types[i] != type)
				Error("Does not match trait", *compiler->current_function->current_token);
			types[i] = type;
			was_template = true;
			printf("found the template type!");
		}
		i++;
	}
	if (was_template == false && match_type->name.back() == '>')
	{
		auto traits = type->GetTraits(compiler);
		//see if it is templated, if it is, look through its templates
		std::string basename = match_type->name.substr(0, match_type->name.length() - 3);
		for (auto ii : traits)
		{
			if (ii.second->name == basename)
			{
				printf("it matches the trait!");
				match_type->Load(compiler);
				for (int i = 0; i < type->data->template_args.size(); i++)
				{
					FindTemplates(compiler, types, type->data->template_args[i], match_type->trait->template_args[i], ii.second);
				}			

				//check if template values match any
				//ok, now see what the template values are
				break;
			}
		}
		printf("loaded yo");
	}
}

std::vector<std::pair<Type**,Trait*>> Type::GetTraits(Compiler* compiler)
{
	if (this->traits.size())
		return this->traits;

	//load traits yo
	for (auto ii : compiler->traits)
	{
		if (this->type == Types::Struct)
		{
			if (ii.second->templates.size())
			{
				//first get a list 
				//this is gonna be fun
				//need to look for matching type sets
				//infer templates
				Type** types = new Type*[ii.second->templates.size()];
				for (int i = 0; i < ii.second->templates.size(); i++)
					types[i] = 0;// compiler->LookupType("int");//cheat for now

				bool match = true;
				for (auto fun : ii.second->funcs)
				{
					auto range = this->data->functions.equal_range(fun.first);
					if (range.first == range.second)
					{
						match = false;
						break;//couldnt find it, doesnt match
					}

					//try and infer types
					//lets just go with first option for the moment
					//printf("inferring types...");

					if (range.first->second->return_type)
						FindTemplates(compiler, types, range.first->second->return_type, fun.second->return_type, ii.second);

					//do it for args
					for (int i = 1; i < range.first->second->argst.size(); i++)
						FindTemplates(compiler, types, range.first->second->argst[i].first, fun.second->argst[i - 1].first, ii.second);
					/*if (range.first->second->return_type)
					{
						//check if return type is the template
						int i = 0;
						bool was_template = false;
						for (auto temp : ii.second->templates)
						{
							if (fun.second->return_type->name == temp.second)
							{
								types[i] = range.first->second->return_type;
								//range.first->second->return_type->name
								was_template = true;
								printf("found the template type!");
							}
							i++;
						}
						if (was_template == false && fun.second->return_type->name.back() == '>')
						{
							auto traits = range.first->second->return_type->GetTraits(compiler);
							//see if it is templated, if it is, look through its templates
							std::string basename = fun.second->return_type->name.substr(0, fun.second->return_type->name.length() - 3);
							for (auto ii : traits)
							{
								if (ii.second->name == basename)
								{
									printf("it matches the trait!");

									//check if template values match any
									//ok, now see what the template values are
									break;
								}
							}
							printf("loaded yo");
						}
					}*/
				}
				if (match)
					this->traits.push_back({ types, ii.second });
				continue;
			}
			bool match = true;
			for (auto fun : ii.second->funcs)
			{
				auto range = this->data->functions.equal_range(fun.first);
				if (range.first == range.second)
				{
					match = false;
					break;//couldnt find it, doesnt match
				}
				for (auto f = range.first; f != range.second; f++)
				{
					if (IsMatch(fun.second, f->second) == false)
					{
						match = false;
						break;
					}
				}
			}
			if (match == false)
				continue;
		}
		else
		{
			if (ii.second->funcs.size())
				continue;//only add if no functions
		}
		this->traits.push_back({ 0, ii.second });
	}
	return this->traits;
}

bool Type::MatchesTrait(Compiler* compiler, Trait* t)
{
	auto ttraits = this->GetTraits(compiler);
	bool found = false;
	for (auto tr : ttraits)
	{
		if (tr.second == t)
		{
			found = true;
			break;
		}
	}
	return found;
}

Type* Type::Instantiate(Compiler* compiler, const std::vector<Type*>& types)
{
	//register the types
	int i = 0;
	for (auto ii : this->data->templates)
	{
		//check if traits match
		if (types[i]->MatchesTrait(compiler, ii.first->trait) == false)
			Error("Type '" + types[i]->name + "' doesn't match Trait '" + ii.first->name + "'", *compiler->current_function->current_token);

		//lets be stupid and just register the type
		//CHANGE ME LATER, THIS OVERRIDES TYPES, OR JUST RESTORE AFTER THIS
		compiler->types[ii.second] = types[i++];
	}

	//duplicate and load
	Struct* str = new Struct;
	//build members
	for (auto ii : this->data->expression->members)
	{
		if (ii.type == StructMember::VariableMember)
		{
			auto type = compiler->AdvanceTypeLookup(ii.variable.first.text);

			str->members.push_back({ ii.variable.second.text, ii.variable.first.text, type });
		}
	}

	str->template_args = types;
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

	//add on items from parent
	if (this->parent)
	{
		this->parent->Load(compiler);

		if (this->parent->type != Types::Struct)
			Error("Struct's parent must be another Struct!", *compiler->current_function->current_token);

		//add its members to me
		auto oldmem = std::move(this->members);
		this->members.clear();
		for (auto ii : this->parent->data->members)
			this->members.push_back(ii);
		for (auto ii : oldmem)
			this->members.push_back(ii);

		//add the functions too :D
		auto oldfuncs = std::move(this->functions);
		this->functions.clear();
		for (auto ii : this->parent->data->functions)
			this->functions.insert(ii);
	}

	//recursively load
	std::vector<llvm::Type*> elementss;
	for (auto ii : this->members)
	{
		auto type = ii.type;
		ii.type->Load(compiler);

		elementss.push_back(GetType(type));
	}
	if (elementss.size() == 0)
		Error("Struct contains no elements!! Fix this not being ok!", *compiler->current_function->current_token);

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

Function* Function::Instantiate(Compiler* compiler, const std::vector<Type*>& types)
{
	/*//register the types
	int i = 0;
	for (auto ii : this->templates)
	{
	//check if traits match
	if (types[i]->MatchesTrait(compiler, ii.first) == false)
	Error("Type '" + types[i]->name + "' doesn't match Trait '" + ii.first->name + "'", *compiler->current_function->current_token);

	//lets be stupid and just register the type
	//CHANGE ME LATER, THIS OVERRIDES TYPES, OR JUST RESTORE AFTER THIS
	compiler->types[ii.second] = types[i++];
	}

	//duplicate and load
	Function* str = new Function;
	//build members
	for (auto ii : this->expression->members)
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

	return t; */
	return 0;
}