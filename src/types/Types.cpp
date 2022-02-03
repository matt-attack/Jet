#include "Types.h"
#include "../Compiler.h"
#include "../CompilerContext.h"
#include <expressions/Expressions.h>
#include <expressions/DeclarationExpressions.h>
#include "../Lexer.h"
#include "Function.h"

using namespace Jet;

Type* Type::GetPointerType()
{
	if (this->pointer_type)
		return this->pointer_type;

	auto type = new Type(compilation, name + "*", Types::Pointer);
	type->base = this;
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
		this->debug_type = compiler->debug->createBasicType("bool", 8, llvm::dwarf::DW_ATE_boolean);
	else if (this->type == Types::Int)
		this->debug_type = compiler->debug->createBasicType("int", 32, llvm::dwarf::DW_ATE_signed);
	else if (this->type == Types::UInt)
		this->debug_type = compiler->debug->createBasicType("uint", 32, llvm::dwarf::DW_ATE_unsigned);
	else if (this->type == Types::Short)
		this->debug_type = compiler->debug->createBasicType("short", 16, llvm::dwarf::DW_ATE_signed);
	else if (this->type == Types::UShort)
		this->debug_type = compiler->debug->createBasicType("ushort", 16, llvm::dwarf::DW_ATE_unsigned);
	else if (this->type == Types::Char)
		this->debug_type = compiler->debug->createBasicType("char", 8, llvm::dwarf::DW_ATE_signed_char);
	else if (this->type == Types::UChar)
		this->debug_type = compiler->debug->createBasicType("uchar", 8, llvm::dwarf::DW_ATE_unsigned_char);
	else if (this->type == Types::Long)
		this->debug_type = compiler->debug->createBasicType("long", 64, llvm::dwarf::DW_ATE_signed);
	else if (this->type == Types::ULong)
		this->debug_type = compiler->debug->createBasicType("ulong", 64, llvm::dwarf::DW_ATE_unsigned);
	else if (this->type == Types::Float)
		this->debug_type = compiler->debug->createBasicType("float", 32, llvm::dwarf::DW_ATE_float);
	else if (this->type == Types::Double)
		this->debug_type = compiler->debug->createBasicType("double", 64, llvm::dwarf::DW_ATE_float);
	else if (this->type == Types::Pointer)
		this->debug_type = compiler->debug->createPointerType(this->base->GetDebugType(compiler), compilation->pointer_size*8);//todo handle 64 bit
	else if (this->type == Types::Function)
		this->debug_type = compiler->debug->createBasicType("fun_pointer", 64, llvm::dwarf::DW_ATE_address);
	else if (this->type == Types::Struct)
	{
		llvm::DIType* typ = 0;

		int line = 0;
		if (this->data->expression)
			line = this->data->expression->token.line;

        int real_size = 0;
		for (auto type : this->data->struct_members)
		{
			type.type->Load(compiler);
			real_size += type.type->GetSize()*8;
		}

		auto dt = compiler->debug->createStructType(compiler->debug_info.file, this->data->name, compiler->debug_info.file, line, real_size, 8, llvm::DINode::DIFlags::FlagPublic, typ, 0);
		this->debug_type = dt;

		//now build and set elements
		std::vector<llvm::Metadata*> ftypes;
		int offset = 0;
		for (auto type : this->data->struct_members)
		{
			//assert(type.type->loaded);
			type.type->Load(compiler);
			int size = type.type->GetSize();
			auto mt = compiler->debug->createMemberType(compiler->debug_info.file, type.name, compiler->debug_info.file, line, size * 8, 8, offset * 8, llvm::DINode::DIFlags::FlagPublic, type.type->GetDebugType(compiler));

			ftypes.push_back(mt);// type.type->GetDebugType(compiler));
			offset += size;
		}
		dt->replaceElements(compiler->debug->getOrCreateArray(ftypes));
	}
	else if (this->type == Types::Union)
	{
		this->debug_type = compiler->debug->createUnionType(compiler->debug_info.file, this->name, compiler->debug_info.file, 0, 512, 8, llvm::DINode::DIFlags::FlagPublic, {});
	}
	else if (this->type == Types::Function)
	{
		std::vector<llvm::Metadata*> ftypes;
		for (auto type : this->function->args)
			ftypes.push_back(type->GetDebugType(compiler));

		this->debug_type = compiler->debug->createSubroutineType(/*compiler->debug_info.file,*/ compiler->debug->getOrCreateTypeArray(ftypes));
	}
	else if (this->type == Types::Array)
	{
		llvm::DIType* typ = 0;

		int line = 0;
		//if (this->data->expression)
		//	line = this->data->expression->token.line;

        int real_size = 0;
		auto list = { compiler->IntType, this->base->GetPointerType() };
		for (auto type : list)
		{
			type->Load(compiler);
			real_size += type->GetSize()*8;
		}

		auto dt = compiler->debug->createStructType(compiler->debug_info.file, this->name, compiler->debug_info.file, line, real_size, 8, llvm::DINode::DIFlags::FlagPublic, typ, 0);
		this->debug_type = dt;

		//now build and set elements
		std::vector<llvm::Metadata*> ftypes;
		int offset = 0;
		const char* names[] = { "size", "ptr" };
		int i = 0;
		for (auto type : list)
		{
			type->Load(compiler);
			int size = type->GetSize();
			auto mt = compiler->debug->createMemberType(compiler->debug_info.file, names[i++], compiler->debug_info.file, line, size * 8, 8, offset * 8, llvm::DINode::DIFlags::FlagPublic, type->GetDebugType(compiler));

			ftypes.push_back(mt);
			offset += size;
		}
		dt->replaceElements(compiler->debug->getOrCreateArray(ftypes));


		this->debug_type = dt;
	}
	else if (this->type == Types::InternalArray)
	{
		auto dt = compiler->debug->createArrayType(this->size, 8, this->base->GetDebugType(compiler), {});
		this->debug_type = dt;
	}

	if (this->debug_type)
		return this->debug_type;

	compiler->Error("Compiler Error: GetDebugType not implemented for type", compiler->current_function->current_token);
	throw 7;
}

llvm::Type* Type::GetLLVMType()
{
	switch ((int)this->type)
	{
	case (int)Types::Double:
		return llvm::Type::getDoubleTy(compilation->context);
	case (int)Types::Float:
		return llvm::Type::getFloatTy(compilation->context);
	case (int)Types::UInt:
	case (int)Types::Int:
		return llvm::Type::getInt32Ty(compilation->context);
	case (int)Types::ULong:
	case (int)Types::Long:
		return llvm::Type::getInt64Ty(compilation->context);
	case (int)Types::Void:
		return llvm::Type::getVoidTy(compilation->context);
	case (int)Types::UChar:
	case (int)Types::Char:
		return llvm::Type::getInt8Ty(compilation->context);
	case (int)Types::UShort:
	case (int)Types::Short:
		return llvm::Type::getInt16Ty(compilation->context);
	case (int)Types::Bool:
		return llvm::Type::getInt1Ty(compilation->context);
	case (int)Types::Struct:
		assert(this->loaded);
		return this->data->type;
	case (int)Types::Array:
	{
		if (this->llvm_type)
			return this->llvm_type;

		std::vector<llvm::Type*> types = { llvm::Type::getInt32Ty(compilation->context),
                                           base->GetPointerType()->GetLLVMType() };
		auto res = llvm::StructType::create(types, this->name, true);
		this->llvm_type = res;
		return res;
	}
	case (int)Types::InternalArray:
	{
		auto res = llvm::ArrayType::get(base->GetLLVMType(), this->size);//todo: use real size here
		return res;
	}
	case (int)Types::Pointer:
		return llvm::PointerType::get(this->base->GetLLVMType(), 0);//address space, wat?
	case (int)Types::Function:
	{
        // if we return a struct, make sure to use that as the first argument
        if (this->function->return_type->type == Types::Struct)
        {
		    std::vector<llvm::Type*> args;
            args.push_back(this->function->return_type->GetPointerType()->GetLLVMType());
		    for (auto ii : this->function->args)
			    args.push_back(ii->GetLLVMType());

		    return llvm::FunctionType::get(llvm::Type::getVoidTy(compilation->context), args, false)->getPointerTo();
        }

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
		printf("ERROR: GetLLVMType not implemented for type!\n");
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
		auto id_t = llvm::IntegerType::get(compiler->context, 32);// For the moment use a 32 bit tag
		auto base = llvm::ArrayType::get(char_t, size);
		this->_union->type = llvm::StructType::get(compiler->context, { id_t, base }, true);
	}
	else if (type == Types::Invalid)
	{
		compiler->Error("Tried To Use Undefined Type '" + this->name + "'", compiler->current_function->current_token);
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
	if (a->return_type_ != b->return_type_)
		return false;

	if (a->arguments_.size() != b->arguments_.size() - 1)
		return false;

	for (int i = 1; i < a->arguments_.size(); i++)
		if (a->arguments_[i].first != b->arguments_[i - 1].first)
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

					if (range.first->second->return_type_)
					{
						bool res = FindTemplates(compiler, types, range.first->second->return_type_, fun.second->return_type_, ii.second, fun.second->return_type_->name);
						if (res == false)
						{
							match = false;
							break;
						}

						if (fun.second->return_type_->type != Types::Trait && fun.second->return_type_->type != Types::Invalid && fun.second->return_type_ != range.first->second->return_type_)
						{
							match = false;
							break;
						}
					}

					//do it for args
					if (range.first->second->arguments_.size() != fun.second->arguments_.size() + 1)
					{
						match = false;
						break;
					}
					for (int i = 1; i < range.first->second->arguments_.size(); i++)
					{
						bool res = FindTemplates(compiler, types, range.first->second->arguments_[i].first, fun.second->arguments_[i - 1].first, ii.second, fun.second->arguments_[i - 1].first->name);
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
		Type* trait = new Type(compiler, "", Types::Trait);
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
		compiler->Error("Incorrect number of template arguments. Got " + std::to_string(types.size()) + " Expected " + std::to_string(this->data->templates.size()), compiler->current_function->current_token);

	//duplicate and load
	Struct* str = new Struct;

	//create a new namespace here for the struct
	str->parent = this->ns;
	auto oldns = compiler->ns;
	compiler->ns = str;

	//register the types
	bool has_trait_template_arg = false;
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

		if (types[i]->type == Types::Trait)
			has_trait_template_arg = true;

		//check if traits match
		if (types[i]->MatchesTrait(compiler, ii.first->trait) == false)
			compiler->Error("Type '" + types[i]->name + "' doesn't match Trait '" + ii.first->name + "'", compiler->current_function->current_token);

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


	Type* t = new Type(compilation, str->name, Types::Struct, str);
	t->ns = this->ns;

	//make sure the real thing is stored as this
	auto realname = t->ToString();
	//oh, this is wrong
	//uh oh, this duplicates
	if (has_trait_template_arg)
	{
		t->name += '^';
		realname += '^';
	}
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
	{
		t->ns->members.insert({ realname, t });
	}

	//build members
	for (auto ii : this->data->expression->members)
	{
		if (ii.type == StructMember::VariableMember)
		{
			auto type = compiler->LookupType(ii.variable.type.text, false);

			str->struct_members.push_back({ ii.variable.name.text, ii.variable.type.text, type });
		}
	}

	//compile its functions
	if (t->data->expression->members.size() > 0 && (compiler->typecheck == false || t->IsValid()))// t->data->template_args[0]->type != Types::Trait)
	{//need to still do this when typechecking, if the type actually can be instantiated
		t->Load(compiler);
		//fixme, if im typechecking no functions get filled in

		StructExpression* expr = dynamic_cast<StructExpression*>(t->data->expression);
		auto oldname = expr->name;
		expr->name.text = t->data->name;
        expr->namespaceprefix_ = "";
        expr->ResetNamespace();

		int start = compiler->unresolved_types.size();

		//store then restore insertion point
		auto rp = compiler->builder.GetInsertBlock();
		auto dp = compiler->builder.getCurrentDebugLocation();

		for (auto ii : t->data->expression->members)
			if (ii.type == StructMember::FunctionMember)
            {
                ii.function->ResetNamespace();
				ii.function->CompileDeclarations(compiler->current_function);
            }

		expr->AddConstructorDeclarations(t, compiler->current_function);

		//fixme later, compiler->types should have a size of 0 and this should be unnecesary
		assert(start == compiler->unresolved_types.size());

		for (auto ii : t->data->expression->members)
			if (ii.type == StructMember::FunctionMember)
            {
				ii.function->DoCompile(compiler->current_function);//the context used may not be proper, but it works
                ii.function->ResetNamespace();
            }

		expr->AddConstructors(compiler->current_function);

		compiler->builder.SetCurrentDebugLocation(dp);
		if (rp)
			compiler->builder.SetInsertPoint(rp);
		expr->name = oldname;
        expr->namespaceprefix_ = "";
        expr->ResetNamespace();
	}
	else
	{
		StructExpression* expr = dynamic_cast<StructExpression*>(t->data->expression);

		auto oldname = expr->name;
		expr->name.text = t->data->name;
        expr->namespaceprefix_ = "";
        expr->ResetNamespace();

		//need this when typechecking
		for (auto ii : t->data->expression->members)
			if (ii.type == StructMember::FunctionMember)
				ii.function->CompileDeclarations(compiler->current_function);

		//it needs dummy constructors too
		expr->AddConstructorDeclarations(t, compiler->current_function);

		bool has_trait = false;
		for (unsigned int i = 0; i < types.size(); i++)
		{
			if (types[i]->IsValid() == false)
			{
				has_trait = true;
				break;
			}
		}

		if (has_trait == false)
			compiler->unfinished_templates.push_back(t);

		expr->name = oldname;
        expr->namespaceprefix_ = "";
        expr->ResetNamespace();
	}

	//remove myself from my ns if im a trait
	if (has_trait_template_arg)
	{
		//todo do I need this? it was commented out
		//t->ns->members.erase(t->ns->members.find(realname));
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
	else if (this->type == Types::Pointer)
	{
		Type* base = this->base;
		while (base->type == Types::Pointer)
			base = base->base;

		return base->IsValid();
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
	case (int)Types::InternalArray:
		return this->base->ToString() + "[" + std::to_string(this->size) + "]";
	case (int)Types::Array:
		return this->base->ToString() + "[]";
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
			else if (ii->second->arguments_.size() + 1 == args.size())
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
				else if (ii->second->arguments_.size() + 1 == args.size())
					fun = ii->second;
			}
		}

		return fun;
	}

	if (this->type != Types::Struct)
    {
		return 0;
    }

	//im a struct yo
	Function* fun = 0;
	auto range = this->data->functions.equal_range(name);
	for (auto ii = range.first; ii != range.second; ii++)
	{
		//pick one with the right number of args
		if (def)
			fun = ii->second;
		else if (ii->second->arguments_.size() == args.size())
			fun = ii->second;
	}

    if (fun)
    {
        return fun;
    }

	//check for uncompiled trait extension methods and compile them if we find em
	auto ttraits = this->GetTraits(context->root);
	for (auto tr : ttraits)
	{
		auto frange = tr.second->extension_methods.equal_range(name);
		for (auto ii = frange.first; ii != frange.second; ii++)
		{
			//pick one with the right number of args
			if (def || ii->second->arguments_.size() == args.size())
				fun = ii->second;
		}

		if (fun)
		{
			auto ns = new Namespace;
			ns->name = tr.second->name;
			ns->parent = context->root->ns;
			context->root->ns = ns;

			context->root->ns->members.insert({ tr.second->name, this });

            auto exp = fun->expression_;


			auto rp = context->root->builder.GetInsertBlock();
			auto dp = context->root->builder.getCurrentDebugLocation();

			//compile function
			auto oldn = exp->Struct.text;
			exp->Struct.text = this->name;
			int i = 0;
			for (auto ii : tr.second->templates)
				context->root->ns->members.insert({ ii.second, tr.first[i++] });

			exp->CompileDeclarations(context);
			exp->DoCompile(context);

			context->root->ns->members.erase(context->root->ns->members.find(tr.second->name));

			context->root->ns = context->root->ns->parent;
			exp->Struct.text = oldn;

			context->root->builder.SetCurrentDebugLocation(dp);
			context->root->builder.SetInsertPoint(rp);

			fun = this->data->functions.find(name)->second;

			break;
		}
	}
	return fun;
}

int Type::GetSize()
{
	if (this->type == Types::Struct)
	{
		int size = 0;
		for (unsigned int i = 0; i < this->data->struct_members.size(); i++)
			size += this->data->struct_members[i].type->GetSize();
		return size;
	}
	switch (this->type)
	{
	case Types::UInt:
	case Types::Int:
		return 4;
	case Types::Short:
	case Types::UShort:
		return 2;
	case Types::Char:
	case Types::UChar:
		return 1;
	case Types::ULong:
	case Types::Long:
	case Types::Double:
		return 8;
	case Types::Pointer:
		return compilation->pointer_size;//todo: use correct size for 64 bit when that happens
	case Types::Array:
		return 12;//pointer + integer todo need to use pointer size here too
	case Types::InternalArray:
		return this->base->GetSize()*this->size;
	}
	return 4;//todo
}

llvm::Constant* Type::GetDefaultValue(Compilation* compilation)
{
	llvm::Constant* initializer = 0;
	if (this->IsInteger())
	{
		initializer = llvm::ConstantInt::get(compilation->context, llvm::APInt(this->GetSize()*8, 0, this->IsSignedInteger()));
	}
	else if (this->type == Types::Double)
		initializer = llvm::ConstantFP::get(compilation->context, llvm::APFloat(0.0));
	else if (this->type == Types::Float)
		initializer = llvm::ConstantFP::get(compilation->context, llvm::APFloat(0.0f));
	else if (this->type == Types::Pointer)
		initializer = llvm::ConstantPointerNull::get(llvm::dyn_cast<llvm::PointerType>(this->GetLLVMType()));
	else if (this->type == Types::Array)
	{
		//todo this seems wrong...
		std::vector<llvm::Constant*> arr;
		arr.push_back(this->base->GetDefaultValue(compilation));
		initializer = llvm::ConstantArray::get(llvm::dyn_cast<llvm::ArrayType>(this->GetLLVMType()), arr);
	}
	else if (this->type == Types::Struct)
	{
		std::vector<llvm::Constant*> arr;
		for (auto ii : this->data->struct_members)
			arr.push_back(ii.type->GetDefaultValue(compilation));
		if (this->data->struct_members.size() == 0)
		{
			//add the padding value
			arr.push_back(llvm::ConstantInt::get(compilation->context, llvm::APInt(32, 0, true)));
		}
		initializer = llvm::ConstantStruct::get(llvm::dyn_cast<llvm::StructType>(this->GetLLVMType()), arr);
	}
	else if (this->type == Types::InternalArray)
	{
		std::vector<llvm::Constant*> arr;
		arr.push_back(this->base->GetDefaultValue(compilation));
		initializer = llvm::ConstantArray::get(llvm::dyn_cast<llvm::ArrayType>(this->GetLLVMType()), arr);
	}
	else
	{
		printf("Unhandled Type in Type::GetDefaultValue!\n");
		throw 7;
	}
	return initializer;
}
