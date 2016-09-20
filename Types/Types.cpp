#include "Types.h"
#include "Compiler.h"
#include "CompilerContext.h"
#include "Expressions.h"
#include "DeclarationExpressions.h"
#include "Lexer.h"
#include "Function.h"

//#include <llvm/IR/Attributes.h>
//#include <llvm/IR/Argument.h>
//#include <llvm/ADT/ilist.h>

using namespace Jet;

Type Jet::VoidType("void", Types::Void);

Type* Type::GetPointerType()
{
	if (this->pointer_type)
		return this->pointer_type;

	auto type = new Type;
	type->name = name + "*";
	type->base = this;
	type->type = Types::Pointer;
	this->pointer_type = type;

	type->ns = type->base->ns;
	type->base->ns->members.insert({ type->name, type });

	return type;// tmp;
}

llvm::DIType* Type::GetDebugType(Compilation* compiler)
{
	if (this->loaded == false)
		this->Load(compiler);

	if (this->debug_type)
		return this->debug_type;

	if (this->type == Types::Bool)
		this->debug_type = compiler->debug->createBasicType("bool", 1, 8, llvm::dwarf::DW_ATE_boolean);
	else if (this->type == Types::Int)
		this->debug_type = compiler->debug->createBasicType("int", 32, 32, llvm::dwarf::DW_ATE_signed);
	else if (this->type == Types::UInt)
		this->debug_type = compiler->debug->createBasicType("uint", 32, 32, llvm::dwarf::DW_ATE_unsigned);
	else if (this->type == Types::Short)
		this->debug_type = compiler->debug->createBasicType("short", 16, 16, llvm::dwarf::DW_ATE_signed);
	else if (this->type == Types::UShort)
		this->debug_type = compiler->debug->createBasicType("ushort", 16, 16, llvm::dwarf::DW_ATE_unsigned);
	else if (this->type == Types::Char)
		this->debug_type = compiler->debug->createBasicType("char", 8, 8, llvm::dwarf::DW_ATE_signed_char);
	else if (this->type == Types::UChar)
		this->debug_type = compiler->debug->createBasicType("uchar", 8, 8, llvm::dwarf::DW_ATE_unsigned_char);
	else if (this->type == Types::Long)
		this->debug_type = compiler->debug->createBasicType("long", 64, 64, llvm::dwarf::DW_ATE_signed);
	else if (this->type == Types::ULong)
		this->debug_type = compiler->debug->createBasicType("ulong", 64, 64, llvm::dwarf::DW_ATE_unsigned);
	else if (this->type == Types::Float)
		this->debug_type = compiler->debug->createBasicType("float", 32, 32, llvm::dwarf::DW_ATE_float);
	else if (this->type == Types::Double)
		this->debug_type = compiler->debug->createBasicType("double", 64, 64, llvm::dwarf::DW_ATE_float);
	else if (this->type == Types::Pointer)
		this->debug_type = compiler->debug->createPointerType(this->base->GetDebugType(compiler), 32, 32, "pointer");
	else if (this->type == Types::Function)
		this->debug_type = compiler->debug->createBasicType("fun_pointer", 32, 32, llvm::dwarf::DW_ATE_address);
	else if (this->type == Types::Struct)
	{
		llvm::DIType* typ = 0;

		int line = 0;
		if (this->data->expression)
			line = this->data->expression->token.line;
		auto dt = compiler->debug->createStructType(compiler->debug_info.file, this->data->name, compiler->debug_info.file, line, 1024, 8, 0, typ, 0);
		this->debug_type = dt;

		//now build and set elements
		std::vector<llvm::Metadata*> ftypes;
		int offset = 0;
		for (auto type : this->data->struct_members)
		{
			//assert(type.type->loaded);
			type.type->Load(compiler);
			int size = type.type->GetSize();
			auto mt = compiler->debug->createMemberType(compiler->debug_info.file, type.name, compiler->debug_info.file, line, size*8, 8, offset*8, 0, type.type->GetDebugType(compiler));

			ftypes.push_back(mt);// type.type->GetDebugType(compiler));
			offset += size;
		}
		dt->replaceElements(compiler->debug->getOrCreateArray(ftypes));
	}
	else if (this->type == Types::Union)
	{
		this->debug_type = compiler->debug->createUnionType(compiler->debug_info.file, this->name, compiler->debug_info.file, 0, 512, 8, 0, {});
	}
	else if (this->type == Types::Function)
	{
		std::vector<llvm::Metadata*> ftypes;
		for (auto type : this->function->args)
			ftypes.push_back(type->GetDebugType(compiler));

		this->debug_type = compiler->debug->createSubroutineType(compiler->debug_info.file, compiler->debug->getOrCreateTypeArray(ftypes));
	}
	else if (this->type == Types::Array)
	{
		this->debug_type = compiler->debug->createArrayType(this->size, 8, this->base->GetDebugType(compiler), 0);
	}

	if (this->debug_type)
		return this->debug_type;

	throw 7;
}

llvm::Type* Type::GetLLVMType()
{
	switch ((int)this->type)
	{
	case (int)Types::Double:
		return llvm::Type::getDoubleTy(llvm::getGlobalContext());
	case (int)Types::Float:
		return llvm::Type::getFloatTy(llvm::getGlobalContext());
	case (int)Types::UInt:
	case (int)Types::Int:
		return llvm::Type::getInt32Ty(llvm::getGlobalContext());
	case (int)Types::ULong:
	case (int)Types::Long:
		return llvm::Type::getInt64Ty(llvm::getGlobalContext());
	case (int)Types::Void:
		return llvm::Type::getVoidTy(llvm::getGlobalContext());
	case (int)Types::UChar:
	case (int)Types::Char:
		return llvm::Type::getInt8Ty(llvm::getGlobalContext());
	case (int)Types::UShort:
	case (int)Types::Short:
		return llvm::Type::getInt16Ty(llvm::getGlobalContext());
	case (int)Types::Bool:
		return llvm::Type::getInt1Ty(llvm::getGlobalContext());
	case (int)Types::Struct:
		assert(this->loaded);
		return this->data->type;
	case (int)Types::Array:
		return llvm::ArrayType::get(this->base->GetLLVMType(), this->size);
	case (int)Types::Pointer:
		return llvm::PointerType::get(this->base->GetLLVMType(), 0);//address space, wat?
	case (int)Types::Function:
	{
		std::vector<llvm::Type*> args;
		for (auto ii : this->function->args)
			args.push_back(ii->GetLLVMType());

		return llvm::FunctionType::get(this->function->return_type->GetLLVMType(), args, false)->getPointerTo();
	}
	case (int)Types::Union:
		assert(this->loaded);
		return this->_union->type;
	case (int)Types::Trait:
		return 0;
	default:
		throw 7;//oops
	}
}

std::string Jet::ParseType(const char* tname, int& p)
{
	std::string out;
	while (IsLetter(tname[p]) || IsNumber(tname[p]))
		out += tname[p++];

	//parse namespaces
	while (tname[p] == ':' && tname[p + 1] == ':')
	{
		p += 2;
		out += "::";
		while (IsLetter(tname[p]) || IsNumber(tname[p]))
			out += tname[p++];
	}

	//parse templates
	if (tname[p] == '<')
	{
		p++;
		out += "<";
		//recursively parse the rest
		bool first = true;
		do
		{
			if (first == false)
				out += ",";
			first = false;
			out += ParseType(tname, p);
		} while (tname[p] == ',' && p++);

		p++;
		out += ">";
	}
	else if (tname[p] == '(')
	{
		p++;
		out += "(";
		//recursively parse the rest
		bool first = true;
		do
		{
			if (first == false)
				out += ",";
			first = false;
			out += ParseType(tname, p);
		} while (tname[p] == ',' && p++);

		p++;
		out += ")";
	}

	while (tname[p] == '*')//parse pointers
	{
		out += '*';
		p++;
	}
	return out;
}

void Type::Load(Compilation* compiler)
{
	//recursively load
	if (loaded == true)
		return;

	if (type == Types::Struct)
	{
		data->Load(compiler);
	}
	else if (type == Types::Union)
	{
		//build the type
		int size = 0;
		for (auto ii : this->_union->members)
		{
			int s = ii->GetSize();
			if (s > size)
				size = s;
		}

		//allocate a struct type with the right size
		auto char_t = llvm::IntegerType::get(compiler->context, 8);
		auto id_t = llvm::IntegerType::get(compiler->context, 32);
		auto base = llvm::ArrayType::get(char_t, size);
		this->_union->type = llvm::StructType::get(compiler->context, { id_t, base }, true);
	}
	else if (type == Types::Invalid)
	{
		compiler->Error("Tried To Use Undefined Type '" + this->name + "'", *compiler->current_function->current_token);
	}
	else if (type == Types::Pointer)
	{
		//load recursively
		this->base->Load(compiler);
		this->ns = this->base->ns;
	}
	this->loaded = true;
}


//is is function, b is trait fruncton
bool IsMatch(Function* a, Function* b)
{
	if (a->return_type != b->return_type)
		return false;

	if (a->arguments.size() != b->arguments.size() - 1)
		return false;

	for (int i = 1; i < a->arguments.size(); i++)
		if (a->arguments[i].first != b->arguments[i - 1].first)
			return false;

	return true;
}

bool FindTemplates(Compilation* compiler, Type** types, Type* type, Type* match_type, Trait* trait, const std::string& match_name)
{
	int i = 0;
	bool was_template = false;
	for (auto temp : trait->templates)
	{
		if (match_name/*match_type->name*/ == temp.second)
		{
			if (types[i] && types[i] != type)
				return false;// Error("Does not match trait", *compiler->current_function->current_token);
			types[i] = type;
			was_template = true;
		}
		i++;
	}

	if (was_template == false && match_type->name.back() == '>')
	{
		auto traits = type->GetTraits(compiler);
		//see if it is templated, if it is, look through its templates
		std::string basename = match_type->name.substr(0, match_type->name.find_first_of('<'));// match_type->name.length() - 3);
		for (auto ii : traits)
		{
			if (ii.second->name == basename)
			{
				match_type->Load(compiler);

				//ok, we now have what template args are used in the trait, see if any are the ones we are looking for
				for (int x = 0; x < match_type->trait->template_args.size(); x++)
				{
					for (int i = 0; i < ii.second->templates.size();/* type->data->template_args.size();*/ i++)
					{
						bool res = FindTemplates(compiler, types, ii.first[i], match_type->trait->template_args[x], ii.second, match_type->trait->templates[x].second);
						if (res == false)
							return false;
					}
				}

				return true;
				break;
			}
		}

		//otherwise try and determine type in the template
	}
	return true;
}

std::vector<std::pair<Type**, Trait*>> Type::GetTraits(Compilation* compiler)
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
				//need to look for matching type sets
				//infer templates
				Type** types = new Type*[ii.second->templates.size()];
				for (int i = 0; i < ii.second->templates.size(); i++)
					types[i] = 0;//cheat for now

				bool match = true;
				for (auto fun : ii.second->functions)
				{
					auto range = this->data->functions.equal_range(fun.first);
					if (range.first == range.second)
					{
						match = false;
						break;//couldnt find it, doesnt match
					}

					if (range.first->second->return_type)
					{
						bool res = FindTemplates(compiler, types, range.first->second->return_type, fun.second->return_type, ii.second, fun.second->return_type->name);
						if (res == false)
						{
							match = false;
							break;
						}

						if (fun.second->return_type->type != Types::Trait && fun.second->return_type->type != Types::Invalid && fun.second->return_type != range.first->second->return_type)
						{
							match = false;
							break;
						}
					}

					//do it for args
					if (range.first->second->arguments.size() != fun.second->arguments.size() + 1)
					{
						match = false;
						break;
					}
					for (int i = 1; i < range.first->second->arguments.size(); i++)
					{
						bool res = FindTemplates(compiler, types, range.first->second->arguments[i].first, fun.second->arguments[i - 1].first, ii.second, fun.second->arguments[i - 1].first->name);
						if (res == false)
						{
							match = false;
							break;
						}
					}
				}

				bool not_found = false;
				for (int i = 0; i < ii.second->templates.size(); i++)
					if (types[i] == 0)
						not_found = true;
				if (match && not_found == false)
					this->traits.push_back({ types, ii.second });
				continue;
			}

			bool match = true;
			for (auto fun : ii.second->functions)
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
			if (ii.second->functions.size())
				continue;//only add if no functions
		}
		this->traits.push_back({ 0, ii.second });
	}
	return this->traits;
}

bool Type::MatchesTrait(Compilation* compiler, Trait* t)
{
	if (compiler->typecheck && this->type == Types::Trait && this->trait == t)
		return true;

	if (this->type == Types::Trait)
		return false;

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

Type* Type::Instantiate(Compilation* compiler, const std::vector<Type*>& types)
{
	if (this->type == Types::Trait)
	{
		//fix traits referenced in templates
		Type* trait = new Type;
		trait->type = Types::Trait;
		trait->name = this->name + "<";
		int i = 0;
		for (auto ii : types)
		{
			trait->name += ii->name;
			if (i + 1 < types.size())
				trait->name += ",";
		}
		trait->name += ">";
		trait->trait = new Trait;
		*trait->trait = *this->trait;
		trait->trait->template_args = types;

		return trait;
	}

	if (this->data->templates.size() != types.size())
		compiler->Error("Incorrect number of template arguments. Got " + std::to_string(types.size()) + " Expected " + std::to_string(this->data->templates.size()), *compiler->current_function->current_token);

	//duplicate and load
	Struct* str = new Struct;

	//create a new namespace here for the struct
	str->parent = this->ns;
	auto oldns = compiler->ns;
	compiler->ns = str;

	//register the types
	int i = 0;
	for (auto& ii : this->data->templates)
	{
		if (ii.first->trait == 0 || ii.first->type == Types::Invalid)
		{
			//need to make sure to load the trait
			auto oldns = compiler->ns;
			compiler->ns = ii.first->ns;
			ii.first->trait = compiler->LookupType(ii.first->name)->trait;
			compiler->ns = oldns;
		}
		//check if traits match
		if (types[i]->MatchesTrait(compiler, ii.first->trait) == false)
			compiler->Error("Type '" + types[i]->name + "' doesn't match Trait '" + ii.first->name + "'", *compiler->current_function->current_token);

		compiler->ns->members.insert({ ii.second, types[i++] });
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

	//build members
	for (auto ii : this->data->expression->members)
	{
		if (ii.type == StructMember::VariableMember)
		{
			auto type = compiler->LookupType(ii.variable.type.text, false);

			str->struct_members.push_back({ ii.variable.name.text, ii.variable.type.text, type });
		}
	}

	Type* t = new Type(str->name, Types::Struct, str);
	t->ns = this->ns;

	//make sure the real thing is stored as this
	auto realname = t->ToString();

	//uh oh, this duplicates
	auto res = t->ns->members.find(realname);
	if (res != t->ns->members.end() && res->second.type == SymbolType::Type && res->second.ty->type != Types::Invalid)
	{
		delete t;
		t = res->second.ty;

		//restore namespace
		compiler->ns = oldns;

		return t;// goto exit;
	}
	else
		t->ns->members.insert({ realname, t });

	//compile its functions
	if (t->data->expression->members.size() > 0 && (compiler->typecheck == false || t->IsValid()))// t->data->template_args[0]->type != Types::Trait)
	{//need to still do this when typechecking, if the type actually can be instantiated
		t->Load(compiler);
		//fixme, if im typechecking no functions get filled in

		StructExpression* expr = dynamic_cast<StructExpression*>(t->data->expression);
		auto oldname = expr->name;
		expr->name.text = t->data->name;

		int start = compiler->types.size();

		//store then restore insertion point
		auto rp = compiler->builder.GetInsertBlock();
		auto dp = compiler->builder.getCurrentDebugLocation();

		for (auto ii : t->data->expression->members)
			if (ii.type == StructMember::FunctionMember)
				ii.function->CompileDeclarations(compiler->current_function);

		expr->AddConstructorDeclarations(t, compiler->current_function);

		//fixme later, compiler->types should have a size of 0 and this should be unnecesary
		assert(start == compiler->types.size());

		for (auto ii : t->data->expression->members)
			if (ii.type == StructMember::FunctionMember)
				ii.function->DoCompile(compiler->current_function);//the context used may not be proper, but it works

		expr->AddConstructors(compiler->current_function);

		compiler->builder.SetCurrentDebugLocation(dp);
		if (rp)
			compiler->builder.SetInsertPoint(rp);
		expr->name = oldname;
	}
	else
	{
		StructExpression* expr = dynamic_cast<StructExpression*>(t->data->expression);

		auto oldname = expr->name;
		expr->name.text = t->data->name;
		//need this when typechecking
		for (auto ii : t->data->expression->members)
			if (ii.type == StructMember::FunctionMember)
				ii.function->CompileDeclarations(compiler->current_function);

		//it needs dummy constructors too
		expr->AddConstructorDeclarations(t, compiler->current_function);

		bool has_trait = false;
		for (int i = 0; i < types.size(); i++)
			if (types[i]->IsValid() == false)
			{
				has_trait = true;
				break;
			}

		if (has_trait == false)
			compiler->unfinished_templates.push_back(t);

		expr->name = oldname;
	}

	//restore namespace
	compiler->ns = oldns;

	return t;
}

void Type::FinishCompilingTemplate(Compilation* compiler)
{
	if (this->type != Types::Struct)
		throw 7;

	this->Load(compiler);

	StructExpression* expr = dynamic_cast<StructExpression*>(this->data->expression);
	auto oldname = expr->name;
	expr->name.text = this->data->name;

	//store then restore insertion point
	auto rp = compiler->builder.GetInsertBlock();
	auto dp = compiler->builder.getCurrentDebugLocation();

	for (auto ii : this->data->expression->members)
		if (ii.type == StructMember::FunctionMember)
			ii.function->DoCompile(compiler->current_function);//the context used may not be proper, but it works

	expr->AddConstructors(compiler->current_function);

	compiler->builder.SetCurrentDebugLocation(dp);
	if (rp)
		compiler->builder.SetInsertPoint(rp);
	expr->name = oldname;
}

bool Type::IsValid()
{
	if (this->type == Types::Trait)
		return false;
	else if (this->type == Types::Struct && this->data->template_args.size())
	{
		for (auto ii : this->data->template_args)
		{
			if (ii->IsValid() == false)
				return false;
		}
	}
	else if (this->type == Types::Function)
	{
		for (auto ii : this->function->args)
		{
			if (ii->IsValid() == false)
				return false;
		}
		if (this->function->return_type->IsValid() == false)
			return false;
	}
	return true;
}

std::string Type::ToString()
{
	switch ((int)type)
	{
	case (int)Types::Struct:
		return this->data->name;
	case (int)Types::Pointer:
		return this->base->ToString() + "*";
	case (int)Types::Array:
		return this->base->ToString() + "[" + std::to_string(this->size) + "]";
	case (int)Types::Bool:
		return "bool";
	case (int)Types::Char:
		return "char";
	case (int)Types::UChar:
		return "uchar";
	case (int)Types::Int:
		return "int";
	case (int)Types::UInt:
		return "uint";
	case (int)Types::Float:
		return "float";
	case (int)Types::Double:
		return "double";
	case (int)Types::Short:
		return "short";
	case (int)Types::UShort:
		return "ushort";
	case (int)Types::Void:
		return "void";
	case (int)Types::Invalid:
		return "Undefined Type";
	case (int)Types::Trait:
		return this->trait->name;
	case (int)Types::Long:
		return "long";
	case (int)Types::ULong:
		return "ulong";
	case (int)Types::Union:
		return this->name;
	case (int)Types::Function:
		std::string out = this->function->return_type->ToString() + "(";
		bool first = true;
		int i = 0;
		for (auto ii : this->function->args)
		{
			out += ii->ToString();
			if (++i != this->function->args.size())
				out += ",";
		}

		return out + ")";
	}
	assert(false && "Unhandled Type::ToString()");
}

Function* Type::GetMethod(const std::string& name, const std::vector<Type*>& args, CompilerContext* context, bool def)
{
	if (this->type == Types::Trait)
	{
		//only for typechecking, should never hit this during compilation
		//look in this->trait->functions and this->trait->extension_methods
		Function* fun = 0;
		auto range = this->trait->functions.equal_range(name);
		for (auto ii = range.first; ii != range.second; ii++)
		{
			//pick one with the right number of args
			if (def)
				fun = ii->second;
			else if (ii->second->arguments.size() + 1 == args.size())
				fun = ii->second;
		}

		if (fun == 0)
		{
			//check extension_methods
			auto range = this->trait->extension_methods.equal_range(name);
			for (auto ii = range.first; ii != range.second; ii++)
			{
				//pick one with the right number of args
				if (def)
					fun = ii->second;
				else if (ii->second->arguments.size() + 1 == args.size())
					fun = ii->second;
			}
		}

		return fun;
	}
	if (this->type != Types::Struct)
		return 0;

	//im a struct yo
	Function* fun = 0;
	auto range = this->data->functions.equal_range(name);
	for (auto ii = range.first; ii != range.second; ii++)
	{
		//pick one with the right number of args
		if (def)
			fun = ii->second;
		else if (ii->second->arguments.size() == args.size())
			fun = ii->second;
	}

	if (fun == 0)
	{
		//check for trait extension methods
		auto ttraits = this->GetTraits(context->root);
		for (auto tr : ttraits)
		{
			auto frange = tr.second->extension_methods.equal_range(name);
			for (auto ii = frange.first; ii != frange.second; ii++)
			{
				//pick one with the right number of args
				if (def || ii->second->arguments.size() == args.size())
					fun = ii->second;
			}

			if (fun)
			{
				auto ns = new Namespace;
				ns->name = tr.second->name;
				ns->parent = context->root->ns;
				context->root->ns = ns;

				context->root->ns->members.insert({ tr.second->name, this });


				auto rp = context->root->builder.GetInsertBlock();
				auto dp = context->root->builder.getCurrentDebugLocation();

				//compile function
				auto oldn = fun->expression->Struct.text;
				fun->expression->Struct.text = this->name;
				int i = 0;
				for (auto ii : tr.second->templates)
					context->root->ns->members.insert({ ii.second, tr.first[i++] });

				fun->expression->CompileDeclarations(context);
				fun->expression->DoCompile(context);

				context->root->ns->members.erase(context->root->ns->members.find(tr.second->name));

				context->root->ns = context->root->ns->parent;
				fun->expression->Struct.text = oldn;

				context->root->builder.SetCurrentDebugLocation(dp);
				context->root->builder.SetInsertPoint(rp);

				fun = this->data->functions.find(name)->second;

				break;
			}
		}
	}
	return fun;
}

bool is_constructor(const std::string& name)
{
	std::string strname;
	for (int i = 2; i < name.length(); i++)
	{
		if (name[i] == '_')
			break;

		strname.push_back(name[i]);
	}

	auto sub = name.substr(strname.length() + 3, name.length());
	if (name.length() == strname.length() * 2 + 3 && sub == strname)
		return true;
	return false;
}

void Struct::Load(Compilation* compiler)
{
	if (this->loaded)
		return;

	//add on items from parent
	bool needs_vtable = false;
	int vtable_size = 0;
	if (this->parent_struct)
	{
		this->parent_struct->Load(compiler);
		//check parent for vtable
		//	is just moving all the members ok, or do I need to compress vtables
		if (this->parent_struct->type != Types::Struct)
			compiler->Error("Struct's parent must be another Struct!", *compiler->current_function->current_token);

		//add its members to me
		auto oldmem = std::move(this->struct_members);
		this->struct_members.clear();
		//dont add constructors
		for (auto ii : this->parent_struct->data->struct_members)
			this->struct_members.push_back(ii);
		for (auto ii : oldmem)
			this->struct_members.push_back(ii);

		//add the functions too :D
		auto oldfuncs = std::move(this->functions);
		//ok, now remove extra destructors
		this->functions.clear();
		for (auto ii : this->parent_struct->data->functions)
		{
			//setup vtable indices
			this->functions.insert(ii);

			//exclude any constructors, or missing functions
			bool ic = is_constructor(ii.second->name);
			if (ic || ii.first == this->parent_struct->data->name || ii.second == 0)
				continue;

			if (ii.second->virtual_offset + 1 > vtable_size)
				vtable_size = ii.second->virtual_offset + 1;
		}
		for (auto ii : oldfuncs)
		{
			bool duplicate = false; int dup_loc = 0;
			std::string dupname = ii.first.find('~') != -1 ? "~" + this->parent_struct->data->name : ii.first;
			for (auto mem : this->parent_struct->data->functions)
			{
				if (mem.first == dupname)
				{
					duplicate = true;
					dup_loc = mem.second->virtual_offset;

					//lets just erase it
					this->functions.erase(ii.first);
					break;
				}
			}

			//todo have it warn/info if no virtual is suggested
			this->functions.insert(ii);

			//exclude any constructors, or missing functions
			if (ii.first == this->name || ii.second == 0)
				continue;

			assert(ii.second->virtual_offset == -1);

			if (duplicate)
				ii.second->virtual_offset = dup_loc;
			else
				ii.second->virtual_offset = vtable_size++;
		}
	}
	else
	{
		//populate vtable with my own functions
		for (auto ii : this->functions)
		{
			//exclude any constructors, or missing functions
			if (ii.first == this->name || ii.second == 0)
				continue;

			ii.second->virtual_offset = vtable_size++;
		}
	}

	needs_vtable = true;//lets do it always just because (need to only do it when im inherited from)
	if (needs_vtable && this->parent_struct == 0)
	{
		this->struct_members.push_back({ "__vtable", "char*", compiler->LookupType("char**") });//add the member if we dont have it
	}

	//recursively load
	llvm::StructType* struct_type = 0;
	std::vector<llvm::Type*> elementss;
	for (auto ii : this->struct_members)
	{
		auto type = ii.type;

		if (ii.type->type == Types::Struct && ii.type->data == this || ii.type->GetBaseType()->data == this)
		{
			if (ii.type->type == Types::Pointer)
			{
				//we are good, do a cheat load
				if (struct_type == 0)
					struct_type = llvm::StructType::create(compiler->context, this->name);
				elementss.push_back(struct_type->getPointerTo());
			}
			else
				compiler->Error("Circular dependency", *compiler->current_function->current_token);
		}
		else
		{
			ii.type->Load(compiler);

			elementss.push_back(type->GetLLVMType());
		}
	}
	if (elementss.size() == 0)
	{
		//add dummy element
		elementss.push_back(compiler->IntType->GetLLVMType());
	}

	if (struct_type == 0)
		this->type = llvm::StructType::create(elementss, this->name);
	else
	{
		struct_type->setBody(elementss);
		this->type = struct_type;
	}
	this->loaded = true;


	//populate vtable
	//vtables need to be indentical in layout!!!
	if (needs_vtable)
	{
		std::vector<llvm::Constant*> ptrs;
		ptrs.resize(vtable_size);

		//find the vtable location
		int vtable_loc = 0;
		for (; vtable_loc < this->struct_members.size(); vtable_loc++)
		{
			if (this->struct_members[vtable_loc].name == "__vtable")
				break;
		}

		//load the functions first, then add all the virtual ones
		//first add all virtuals from the parent, then all of mine
		for (auto ii : this->functions)
		{
			if (ii.second == 0 || ii.second->virtual_offset == -1)
				continue;

			ii.second->is_virtual = true;
			ii.second->virtual_table_location = vtable_loc;

			ii.second->Load(compiler);
			auto ptr = ii.second->f;
			auto charptr = llvm::ConstantExpr::getBitCast(ptr, compiler->LookupType("char*")->GetLLVMType());

			ptrs[ii.second->virtual_offset] = charptr;
		}

		auto arr = llvm::ConstantArray::get(llvm::ArrayType::get(compiler->LookupType("char*")->GetLLVMType(), vtable_size), ptrs);

		auto oldns = compiler->ns;

		compiler->ns = this;
		//then need to have function calls to virtual functions to go the lookup table
		compiler->AddGlobal("__" + this->name + "_vtable", compiler->LookupType("char*[" + std::to_string(vtable_size) + "]"), arr, true);

		compiler->ns = oldns;
	}
}

Namespace::~Namespace()
{
	for (auto ii : this->members)
	{
		if (ii.second.type == SymbolType::Namespace)
			delete ii.second.ns;
	}
}

void Namespace::OutputMetadata(std::string& data, Compilation* compilation)
{
	//ok, change this to output in blocks, give size and location/namespace
	for (auto ii : this->members)
	{
		//ok, lets add debug location info
		if (ii.second.type == SymbolType::Function && ii.second.fn->do_export)
		{
			data += "extern fun " + ii.second.fn->return_type->ToString() + " ";
			data += ii.first + "(";
			bool first = false;
			for (auto arg : ii.second.fn->arguments)
			{
				if (first)
					data += ",";
				else
					first = true;

				data += arg.first->ToString() + " " + arg.second;
			}
			data += ");";
		}
		else if (ii.second.type == SymbolType::Type)
		{
			if (ii.second.ty->type == Types::Struct)
			{
				if (ii.second.ty->data->template_base)
					continue;//dont bother exporting instantiated templates for now

				//export me
				if (ii.second.ty->data->templates.size() > 0)
				{
					data += "struct " + ii.second.ty->data->name + "<";
					for (int i = 0; i < ii.second.ty->data->templates.size(); i++)
					{
						data += ii.second.ty->data->templates[i].first->name + " ";
						data += ii.second.ty->data->templates[i].second;
						if (i < ii.second.ty->data->templates.size() - 1)
							data += ',';
					}
					data += ">{";
				}
				else
				{
					data += "struct " + ii.second.ty->data->name + "{";
				}
				for (auto var : ii.second.ty->data->struct_members)
				{
					if (var.type == 0 || var.type->type == Types::Invalid)//its a template probably?
					{
						data += var.type_name + " ";
						data += var.name + ";";
					}
					else if (var.type->type == Types::Array)
					{
						data += var.type->base->ToString() + " ";
						data += var.name + "[" + std::to_string(var.type->size) + "];";
					}
					else
					{
						data += var.type->ToString() + " ";
						data += var.name + ";";
					}
				}

				if (ii.second.ty->data->templates.size() > 0 && ii.second.ty->data->template_base == 0)
				{
					//output member functions somehow?
					auto expr = ii.second.ty->data->expression;
					for (auto ii : expr->members)
					{
						if (ii.type == ii.FunctionMember)
						{
							std::string source;
							auto src = ii.function->GetBlock()->start.GetSource(compilation);
							ii.function->Print(source, src);
							data += source;
						}
					}
					data += "}";
					continue;
				}
				data += "}";

				//output member functions
				for (auto fun : ii.second.ty->data->functions)
				{
					data += "extern fun " + fun.second->return_type->ToString() + " " + ii.second.ty->data->name + "::";
					data += fun.first + "(";
					bool first = false;
					for (int i = 1; i < fun.second->arguments.size(); i++)
					{
						if (first)
							data += ",";
						else
							first = true;

						data += fun.second->arguments[i].first->ToString() + " " + fun.second->arguments[i].second;
					}
					data += ");";
				}
			}
			else if (ii.second.ty->type == Types::Trait)
			{
				data += "trait " + ii.second.ty->trait->name;
				for (int i = 0; i < ii.second.ty->trait->templates.size(); i++)
				{
					if (i == 0)
						data += '<';

					data += ii.second.ty->trait->templates[i].second;

					if (i == ii.second.ty->trait->templates.size() - 1)
						data += '>';
					else
						data += ',';
				}
				data += "{";
				for (auto fun : ii.second.ty->trait->functions)
				{
					data += " fun " + fun.second->return_type->name/*ToString()*/ + " " + fun.first + "(";
					bool first = false;
					for (auto arg : fun.second->arguments)
					{
						if (first)
							data += ",";
						else
							first = true;

						data += arg.first->ToString() + " " + arg.second;
					}
					data += ");";
				}
				data += "}";

				//export extension methods
				for (auto fun : ii.second.ty->trait->extension_methods)
				{
					auto source = fun.second->expression->GetBlock()->start.GetSource(compilation);

					data += "fun ";
					fun.second->expression->ret_type.Print(data, source);
					data += " ";
					data += ii.second.ty->trait->name + "::";
					data += fun.first + "(";
					int i = 0;
					for (auto ii : *fun.second->expression->args)
					{
						data += ii.type.text + " " + ii.name.text;
						if (i != fun.second->expression->args->size() - 1)
							data += ", ";
						//ii->Print(output, source);
						i++;
					}
					data += ")";

					fun.second->expression->GetBlock()->Print(data, source);
				}
			}
		}
		else if (ii.second.type == SymbolType::Namespace)
		{
			ii.second.ns->OutputMetadata(data, compilation);
		}
	}
}

int Type::GetSize()
{
	if (this->type == Types::Struct)
	{
		int size = 0;
		for (int i = 0; i < this->data->struct_members.size(); i++)
			size += this->data->struct_members[i].type->GetSize();
		return size;
	}
	return 4;//todo
}


bool Struct::IsParent(Type* ty)
{
	//ok this needs to check down the line
	auto current = this->parent_struct;
	while (current)
	{
		if (current == ty)
			return true;
		current = current->data->parent_struct;
	}

	return false;
}

llvm::Constant* Type::GetDefaultValue(Compilation* compilation)
{
	llvm::Constant* initializer = 0;
	if (this->type == Types::Int || this->type == Types::Short || this->type == Types::Char || this->type == Types::Long)
		initializer = llvm::ConstantInt::get(compilation->context, llvm::APInt(32, 0, true));
	else if (this->type == Types::Double)
		initializer = llvm::ConstantFP::get(compilation->context, llvm::APFloat(0.0));
	else if (this->type == Types::Float)
		initializer = llvm::ConstantFP::get(compilation->context, llvm::APFloat(0.0f));
	else if (this->type == Types::Pointer)
		initializer = llvm::ConstantPointerNull::get(llvm::dyn_cast<llvm::PointerType>(this->GetLLVMType()));
	else if (this->type == Types::Array)
	{
		std::vector<llvm::Constant*> arr;
		arr.push_back(this->base->GetDefaultValue(compilation));
		initializer = llvm::ConstantArray::get(llvm::dyn_cast<llvm::ArrayType>(this->GetLLVMType()), arr);
	}
	else if (this->type == Types::Struct)
	{
		std::vector<llvm::Constant*> arr;
		for (auto ii : this->data->struct_members)
			arr.push_back(ii.type->GetDefaultValue(compilation));
		initializer = llvm::ConstantStruct::get(llvm::dyn_cast<llvm::StructType>(this->GetLLVMType()), arr);
	}
	else
		throw 7;
	return initializer;
}