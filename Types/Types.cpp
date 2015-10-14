#include "Types.h"
#include "Compiler.h"
#include "CompilerContext.h"
#include "Expressions.h"
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
	type->name = name;
	type->base = this;
	type->type = Types::Pointer;
	this->pointer_type = type;

	type->ns = type->base->ns;
	type->base->ns->members.insert({ name, type });

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
	else if (this->type == Types::Short)
		this->debug_type = compiler->debug->createBasicType("short", 16, 16, llvm::dwarf::DW_ATE_signed);
	else if (this->type == Types::Char)
		this->debug_type = compiler->debug->createBasicType("char", 8, 8, llvm::dwarf::DW_ATE_signed_char);
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
		std::vector<llvm::Metadata*> ftypes;
		for (auto type : this->data->struct_members)
		{
			assert(type.type->loaded);
			//fixme later?

			ftypes.push_back(type.type->GetDebugType(compiler));
		}

		llvm::DIType* typ = 0;
		auto file = compiler->debug->createFile(this->data->name,
			compiler->debug_info.cu->getDirectory());

		int line = 0;
		if (this->data->expression)
			line = this->data->expression->token.line;
		//compiler->debug_info.file->dump();
		this->debug_type = compiler->debug->createStructType(compiler->debug_info.file, this->data->name, compiler->debug_info.file, line, 1024, 4, 0, typ, compiler->debug->getOrCreateArray(ftypes));
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
	printf("oops");
}

llvm::Type* Type::GetLLVMType()
{
	switch (this->type)
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
		assert(this->loaded);
		return this->data->type;
	case Types::Array:
		return llvm::ArrayType::get(this->base->GetLLVMType(), this->size);
	case Types::Pointer:
		return llvm::PointerType::get(this->base->GetLLVMType(), 0);//address space, wat?
	case Types::Function:
	{
		std::vector<llvm::Type*> args;
		for (auto ii : this->function->args)
			args.push_back(ii->GetLLVMType());
		return llvm::FunctionType::get(this->function->return_type->GetLLVMType(), args, false)->getPointerTo();
	}
	case Types::Trait:
		return 0;
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
				for (auto fun : ii.second->funcs)
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

						if (fun.second->return_type->type != Types::Trait  && fun.second->return_type->type != Types::Invalid && fun.second->return_type != range.first->second->return_type)
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

bool Type::MatchesTrait(Compilation* compiler, Trait* t)
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
			auto type = compiler->LookupType(ii.variable.first.text, false);

			str->struct_members.push_back({ ii.variable.second.text, ii.variable.first.text, type });
		}
	}

	Type* t = new Type(str->name, Types::Struct, str);
	t->Load(compiler);
	t->ns = this->ns;

	//make sure the real thing is stored as this
	auto realname = t->ToString();

	//uh oh, this duplicates
	auto res = t->ns->members.find(realname);// types.find(realname);
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
	if (t->data->expression->members.size() > 0)
	{
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

	//restore namespace
	compiler->ns = oldns;

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
	case Types::Trait:
		return this->trait->name;
	case Types::Long:
		return "long";
	case Types::Function:
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
	//Error("Unhandled Type::ToString()", Token());
}

Function* Type::GetMethod(const std::string& name, const std::vector<CValue>& args, CompilerContext* context, bool def)
{
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
		//	check for trait extension methods
		auto ttraits = this->GetTraits(context->parent);
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
				ns->parent = context->parent->ns;
				context->parent->ns = ns;

				context->parent->ns->members.insert({ tr.second->name, this });


				auto rp = context->parent->builder.GetInsertBlock();
				auto dp = context->parent->builder.getCurrentDebugLocation();

				//compile function
				auto oldn = fun->expression->Struct.text;
				fun->expression->Struct.text = this->name;
				int i = 0;
				for (auto ii : tr.second->templates)
					context->parent->ns->members.insert({ ii.second, tr.first[i++] });

				fun->expression->CompileDeclarations(context);
				fun->expression->DoCompile(context);

				context->parent->ns->members.erase(context->parent->ns->members.find(tr.second->name));

				context->parent->ns = context->parent->ns->parent;
				fun->expression->Struct.text = oldn;

				context->parent->builder.SetCurrentDebugLocation(dp);
				context->parent->builder.SetInsertPoint(rp);

				fun = this->data->functions.find(name)->second;

				break;
			}
		}
	}
	return fun;
}

void Struct::Load(Compilation* compiler)
{
	if (this->loaded)
		return;

	//add on items from parent
	if (this->parent_struct)
	{
		this->parent_struct->Load(compiler);

		if (this->parent_struct->type != Types::Struct)
			compiler->Error("Struct's parent must be another Struct!", *compiler->current_function->current_token);

		//add its members to me
		auto oldmem = std::move(this->struct_members);
		this->struct_members.clear();
		for (auto ii : this->parent_struct->data->struct_members)
			this->struct_members.push_back(ii);
		for (auto ii : oldmem)
			this->struct_members.push_back(ii);

		//add the functions too :D
		auto oldfuncs = std::move(this->functions);
		this->functions.clear();
		for (auto ii : this->parent_struct->data->functions)
			this->functions.insert(ii);
	}

	//recursively load
	std::vector<llvm::Type*> elementss;
	for (auto ii : this->struct_members)
	{
		auto type = ii.type;
		ii.type->Load(compiler);

		elementss.push_back(type->GetLLVMType());
	}
	if (elementss.size() == 0)
		compiler->Error("Struct contains no elements!! Fix this not being ok!", *compiler->current_function->current_token);

	this->type = llvm::StructType::create(elementss, this->name);

	this->loaded = true;
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
	for (auto ii : this->members)
	{
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
				for (auto fun : ii.second.ty->trait->funcs)
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
						data += ii.first + " " + ii.second;
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