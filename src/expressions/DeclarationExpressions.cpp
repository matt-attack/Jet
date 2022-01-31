#include "Expressions.h"
#include "Compiler.h"
#include "Parser.h"
#include "types/Function.h"
#include "DeclarationExpressions.h"

using namespace Jet;

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DerivedTypes.h>

void EnumExpression::CompileDeclarations(CompilerContext* context)
{
	// Namespace the enum with the name
	context->CurrentToken(&name);
	context->AddAndSetNamespace(name.text);

	int min_value = std::numeric_limits<int>::max();
	int max_value = -1;

	int last_value;//todo: use correct 64 bit type here
	for (auto ii : this->values)
	{
		int cur_value;
		if (ii.value.text.length())
		{
			cur_value = std::atoi(ii.value.text.c_str());//todo: fixme and use actual parsing
		}
		else
		{
			//set it to be one greater than the last
			cur_value = last_value + 1;
		}
		context->root->ns->members.insert({ ii.name.text, new CValue(context->Integer(cur_value)) });
        min_value = std::min(min_value, cur_value);
        max_value = std::max(max_value, cur_value);
		last_value = cur_value;
	}

	// Add max and min values if they dont overlap
    if (context->root->ns->members.find("min") == context->root->ns->members.end())
    {
		context->root->ns->members.insert({ "min", new CValue(context->Integer(min_value)) });
    }
    if (context->root->ns->members.find("max") == context->root->ns->members.end())
    {
		context->root->ns->members.insert({ "max", new CValue(context->Integer(max_value)) });
    }

	context->PopNamespace();
}

CValue ExternExpression::Compile(CompilerContext* context)
{
	return CValue(context->root->VoidType, 0);
}

void ExternExpression::CompileDeclarations(CompilerContext* context)
{
    context->CurrentToken(&type);
    if (type.type == TokenType::Name)
    {
        // its a global variable
        // todo, what do I do?

        bool is_const = false;

		auto var_type = context->root->LookupType(type.text);

		if (type.text.back() == ']')
		{
			std::string len = type.text;
			len = len.substr(len.find_first_of('[') + 1);
		    if (is_const && len.c_str())
		    {
			    context->root->Error("Cannot declare a const global array variable.", token);
		    }

			context->root->AddGlobal(name.text, var_type->base, std::atoi(len.c_str()), 0, false, is_const);
		}
		else
		{
			context->root->AddGlobal(name.text, var_type, 0, 0, false, is_const);
		}

        return;
    }

	std::string fname = name.text;

	// todo come up with a better way to handle c externs
	bool is_c = (token.text == "extern_c");

	Function* fun = new Function(fname, false, is_c, true);
    fun->extern_expression_ = this;
	if (auto attr = dynamic_cast<AttributeExpression*>(this->parent))
	{
		//add the attribute to the Function
		if (attr->name.text == "stdcall")
			fun->calling_convention_ = CallingConvention::StdCall;
		else if (attr->name.text == "thiscall")
			fun->calling_convention_ = CallingConvention::ThisCall;
		else if (attr->name.text == "fastcall")
			fun->calling_convention_ = CallingConvention::FastCall;
	}
	context->root->AdvanceTypeLookup(&fun->return_type_, this->ret_type.text, &this->ret_type);

	// Reserve space for the arguments + this if we apply to a struct
	fun->arguments_.reserve(this->args->size() + (Struct.length() > 0 ? 1 : 0));

	if (Struct.length() > 0)
	{
		// todo this seems wrong
        std::string struct_prefix = Struct;
        // replace :: with __
        for (int i = 0; i < struct_prefix.size(); i++) {
            if (struct_prefix[i] == ':') struct_prefix[i] = '_';
        }
		fun->name_ = "__" + struct_prefix + "_" + fname;//mangled name

		//add to struct
		auto ii = context->root->TryLookupType(Struct);
		if (ii == 0)//its new
		{
			context->root->Error("Tried to declare an extern member function for an undeclared struct!", token);
			//str = new Type;
			//context->root->types[this->name] = str;
		}
		else
		{
			if (ii->type != Types::Struct)
            {
				context->root->Error("Cannot define an extern function for a type that is not a struct", token);
            }

			ii->data->functions.insert({ fname, fun });
		}

		fun->arguments_.push_back({ ii->GetPointerType(), "this" });
	}
	else
	{
		context->root->ns->members.insert({ fname, fun });
	}

	//look up arg types
	for (auto ii : *this->args)
	{
		fun->arguments_.push_back({ 0, ii.name.text });
		context->root->AdvanceTypeLookup(&fun->arguments_.back().first, ii.type.text, &this->token);
	}
}

void TraitExpression::CompileDeclarations(CompilerContext* context)
{
	//check if trait already exists, if its in the table and set as invalid, then we can just fill in the blanks
	Trait* t;
	auto tr = context->root->ns->members.find(name.text);
	if (tr == context->root->ns->members.end())
	{
		t = new Trait;
		Type* ty = new Type(context->root, name.text, Types::Trait);
		ty->trait = t;
		context->root->ns->members.insert({ name.text, Symbol(ty) });
		context->root->traits[name.text] = t;
	}
	else
	{
		context->root->Error("Type '" + name.text + "' already exists", token);
	}

	t->valid = true;
	t->name = this->name.text;
	t->parent = context->root->ns;

	context->root->ns = t;

	//set this as a namespace, add T as a type
	if (this->templates)
	{
		t->templates.reserve(this->templates->size());

		for (auto& ii : *this->templates)
		{
			t->templates.push_back({ 0, ii.name.text });

			auto type = new Type(context->root, ii.name.text, Types::Invalid);
			type->ns = context->root->ns;
			t->members.insert({ ii.name.text, type });
		}
	}

	for (auto ii : this->funcs)
	{
		Function* func = new Function(ii.name.text, false);
		context->root->AdvanceTypeLookup(&func->return_type_, ii.ret_type.text, &ii.ret_type);
		func->arguments_.reserve(ii.args.size());
		for (auto arg : ii.args)
		{
			func->arguments_.push_back({ 0, "dummy" });
			context->root->AdvanceTypeLookup(&func->arguments_.back().first, arg.type.text, &arg.type);
		}

		t->functions.insert({ ii.name.text, func });
	}

	context->root->ns = t->parent;
}
