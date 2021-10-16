#include "StructExpression.h"
#include "FunctionExpression.h"

#include <CompilerContext.h>

using namespace Jet;

StructExpression::~StructExpression()
{
	for (auto ii : members)
		if (ii.type == StructMember::FunctionMember)
			delete ii.function;
		else if (ii.type == StructMember::DefinitionMember)
			delete ii.definition;
}

Type* StructExpression::TypeCheck(CompilerContext* context)
{
	//push the namespace then define these types
	auto me = context->root->LookupType(this->name.text, false);
	auto old = context->root->ns;
	me->data->parent = old;
	context->root->ns = me->data;

	for (auto ii : this->members)
	{
		if (ii.type == StructMember::FunctionMember)
			ii.function->TypeCheck(context);
	}

	context->root->ns = old;

	return 0;
}

void StructExpression::SetParent(Expression* parent)
{
	this->parent = parent;
	for (auto ii : this->members)
	{
		if (ii.type == StructMember::FunctionMember)
			ii.function->SetParent(this);
		else if (ii.type == StructMember::DefinitionMember)
			ii.definition->SetParent(this);
	}
}

void StructExpression::Print(std::string& output, Source* source)
{
	//add tokens for the ( )
	token.Print(output, source);

	name.Print(output, source);
	//output templates
	if (this->templates && this->templates->size() > 0)
	{
		template_open.Print(output, source);// output += "<"; fixme

		for (auto ii : *this->templates)
		{
			ii.type.Print(output, source);
			ii.name.Print(output, source);
			if (ii.comma.text.length())
				ii.comma.Print(output, source);
		}
		template_close.Print(output, source);// output += ">";
	}

	if (this->colon.text.length())
	{
		this->colon.Print(output, source);
		this->base_type.Print(output, source);
	}

	this->start.Print(output, source);

	for (auto ii : this->members)
	{
		if (ii.type == StructMember::FunctionMember)
			ii.function->Print(output, source);
		else if (ii.type == StructMember::DefinitionMember)
			ii.definition->Print(output, source);
		else
		{
			ii.variable.type.Print(output, source);
			ii.variable.name.Print(output, source);
			ii.variable.semicolon.Print(output, source);// output += ";"; fixme
		}
	}

	this->end.Print(output, source);
}

CValue StructExpression::Compile(CompilerContext* context)
{
	if (this->templates)
	{
		//dont compile, just verify that all traits are valid
		for (auto ii : *this->templates)
		{
			auto iter = context->root->TryLookupType(ii.type.text);
			if (iter == 0 || iter->type != Types::Trait)
				context->root->Error("Trait '" + ii.type.text + "' is not defined", ii.type);
		}

		return CValue();
	}

	//compile function members
	for (auto ii : this->members)
	{
		if (ii.type == StructMember::FunctionMember)
			ii.function->Compile(context);
	}

	//add any missing constructors
	this->AddConstructors(context);

	return CValue();
}

void StructExpression::Visit(ExpressionVisitor* visitor)
{
	visitor->Visit(this);

	for (auto ii : members)
	{
		if (ii.type == StructMember::FunctionMember)
			ii.function->Visit(visitor);
		else if (ii.type == StructMember::DefinitionMember)
			ii.definition->Visit(visitor);
	}
}

void StructExpression::AddConstructorDeclarations(Type* str, CompilerContext* context)
{
	//dont need these if we have no members and no base class
	if (this->base_type.text.length() == 0 && this->members.size() == 0)
		return;

	//only do this if we have a virtual function or we have a parent 
	// (can figure out how to not add extra when parent doesnt require us to have it later)
	bool has_virtual = false;
	for (auto ii : this->members)
	{
		if (ii.type == StructMember::FunctionMember && ii.function->token.type == TokenType::Virtual)
		{
			has_virtual = true;
			break;
		}
	}
	if (this->base_type.text.length() == 0 && has_virtual == false)
		return;

	bool has_destructor = false;
	bool has_constructor = false;
	auto strname = str->data->template_base ? str->data->template_base->name : str->data->name;

	for (auto ii : this->members)
	{
		if (ii.type == StructMember::FunctionMember)
		{
			if (ii.function->GetName() == strname)
				has_constructor = true;
			else if (ii.function->GetName() == "~" + strname)
				has_destructor = true;
		}
	}
	//oops im adding extra destructors for imported compiled types :S
	//	becuase im using extern to define them, but I generate destructors beforehand
	//	need to disable this somehow when compiling symbols
	if (has_constructor == false)
	{
		auto fun = new Function("__" + str->data->name + "_" + strname, false);//
		fun->return_type = &VoidType;
		fun->arguments = { { str->GetPointerType(), "this" } };
		fun->f = 0;
		fun->expression = 0;
		fun->type = (FunctionType*)-1;//indicates this is autogenerated
		str->data->functions.insert({ strname, fun });//register function in _Struct
	}
	if (has_destructor == false)
	{
		auto fun = new Function("__" + str->data->name + "_~" + strname, false);//
		fun->return_type = &VoidType;
		fun->arguments = { { str->GetPointerType(), "this" } };
		fun->f = 0;
		fun->expression = 0;
		fun->type = (FunctionType*)-1;//indicates this is autogenerated

		str->data->functions.insert({ "~" + strname, fun });
	}
}

void StructExpression::AddConstructors(CompilerContext* context)
{
	// This fills in autogenerated constructors with ones that initialize vtables
    //return;
	auto Struct = this->GetName();
	//need to initialize virtual tables
	Type* str = context->root->LookupType(Struct);
	std::string strname = str->data->template_base ? str->data->template_base->name : str->data->name;

	for (auto ii : str->data->functions)
	{
        // if its externed, just ignore
        if (ii.second->is_extern)
        {
            continue;
        }

		//need to identify if its a autogenerated fun
		if (ii.second->expression == 0 && ii.second->type == (FunctionType*)-1 
			&& ii.second->name.length() > strname.length() + 2 
			&& ii.second->name.substr(2, strname.length()) == strname)
		{
			//its probably a constructor/destructor we need to fill in
			auto res = ii.second->name.find('~');
			if (res != -1)
			{
				//destructor
				auto rp = context->root->builder.GetInsertBlock();
				auto dp = context->root->builder.getCurrentDebugLocation();

				std::vector<std::pair<Type*, std::string>> argsv;
				argsv.push_back({ str->GetPointerType(), "this" });

				auto ret = &VoidType;

				CompilerContext* function = context->StartFunctionDefinition(ii.second);
				ii.second->f = function->function->f;

				context->root->current_function = function;

				//function->function->context = function;
				//function->function->Load(context->root);
				function->SetDebugLocation(this->token);

				//alloc args
				auto AI = function->function->f->arg_begin();
				for (unsigned Idx = 0, e = argsv.size(); Idx != e; ++Idx, ++AI) {
					// Create an alloca for this variable.
					auto aname = argsv[Idx].second;

					//llvm::IRBuilder<> TmpB(&function->function->f->getEntryBlock(), function->function->f->getEntryBlock().begin());
					//auto Alloca = TmpB.CreateAlloca(argsv[Idx].first->GetLLVMType(), 0, aname);
					// Store the initial value into the alloca.
					//function->root->builder.CreateStore(AI, Alloca);

					AI->setName(aname);

					auto local = context->root->debug->createAutoVariable(
						function->function->scope, aname, context->root->debug_info.file, this->token.line,
						argsv[Idx].first->GetDebugType(context->root));

					llvm::Instruction *call = context->root->debug->insertDeclare(
						AI, local, context->root->debug->createExpression(), 
						llvm::DebugLoc::get(this->token.line, this->token.column, function->function->scope), context->root->builder.GetInsertBlock());
					call->setDebugLoc(llvm::DebugLoc::get(this->token.line, this->token.column, function->function->scope));

					// Add arguments to variable symbol table.
					function->RegisterLocal(aname, CValue(argsv[Idx].first, AI));
				}

				//compile stuff here
				int i = 0;
				for (auto ii : str->data->struct_members)
				{
					if (ii.type->type == Types::Struct)
					{
						//call the destructor
						auto iiname = ii.type->data->template_base ? ii.type->data->template_base->name : ii.type->data->name;
						auto range = ii.type->data->functions.equal_range("~" + iiname);
						for (auto iii = range.first; iii != range.second; iii++)
						{
							if (iii->second->arguments.size() == 1)
							{
								auto myself = function->Load("this");
								std::vector<llvm::Value*> iindex = { context->root->builder.getInt32(0), context->root->builder.getInt32(i) };
								//iii f is missing oops
								auto gep = context->root->builder.CreateGEP(myself.val, iindex, "getmember");
								iii->second->Load(context->root);
								context->root->builder.CreateCall(iii->second->f, gep);
							}
						}
					}
					i++;
				}

				function->Return(CValue());

				context->root->builder.SetCurrentDebugLocation(dp);
				if (rp)
					context->root->builder.SetInsertPoint(rp);

				context->root->current_function = context;

				function->function->Load(context->root);
			}
			else//constructor
			{
				auto rp = context->root->builder.GetInsertBlock();
				auto dp = context->root->builder.getCurrentDebugLocation();

				std::vector<std::pair<Type*, std::string>> argsv;
				argsv.push_back({ str->GetPointerType(), "this" });

				auto ret = &VoidType;

				CompilerContext* function = context->StartFunctionDefinition(ii.second);
				ii.second->f = function->function->f;

				context->root->current_function = function;

				//function->function->context = function;
				//function->function->Load(context->root);
				function->SetDebugLocation(this->token);

				//alloc args
				auto AI = function->function->f->arg_begin();
				for (unsigned Idx = 0, e = argsv.size(); Idx != e; ++Idx, ++AI) {
					// Create an alloca for this variable.
					auto aname = argsv[Idx].second;

					//llvm::IRBuilder<> TmpB(&function->function->f->getEntryBlock(), function->function->f->getEntryBlock().begin());
					//auto Alloca = TmpB.CreateAlloca(argsv[Idx].first->GetLLVMType(), 0, aname);
					// Store the initial value into the alloca.
					//function->root->builder.CreateStore(AI, Alloca);

					AI->setName(aname);

					llvm::DIFile* unit = context->root->debug_info.file;

					auto D = context->root->debug->createAutoVariable(function->function->scope, aname, unit, this->token.line,
						argsv[Idx].first->GetDebugType(context->root));

					llvm::Instruction *Call = context->root->debug->insertDeclare(
						AI, D, context->root->debug->createExpression(), llvm::DebugLoc::get(this->token.line, this->token.column, function->function->scope), context->root->builder.GetInsertBlock());
					Call->setDebugLoc(llvm::DebugLoc::get(this->token.line, this->token.column, function->function->scope));

					// Add arguments to variable symbol table.
					function->RegisterLocal(aname, CValue(argsv[Idx].first, AI));
				}

				//compile stuff here
				int i = 0;
				for (auto ii : str->data->struct_members)
				{
					if (ii.type->type == Types::Struct)
					{
						auto iiname = ii.type->data->template_base ? ii.type->data->template_base->name : ii.type->data->name;

						//call the constructor
						auto range = ii.type->data->functions.equal_range(iiname);
						for (auto iii = range.first; iii != range.second; iii++)
						{
							if (iii->second->arguments.size() == 1)
							{
								auto myself = function->Load("this");
								std::vector<llvm::Value*> iindex = { context->root->builder.getInt32(0), context->root->builder.getInt32(i) };
								//iii f is missing oops
								auto gep = context->root->builder.CreateGEP(myself.val, iindex, "getmember");
								iii->second->Load(context->root);
								context->root->builder.CreateCall(iii->second->f, gep);
							}
						}
					}
					else if (ii.name == "__vtable")//is vtable)
					{
						//first load the global
						//set ns
						auto oldns = function->root->ns;
						function->root->ns = str->data;
						CValue vtable = function->GetVariable("__" + this->name.text + "_vtable");
						function->root->ns = oldns;
						//restore ns

						//cast the global to a char**
						vtable.val = context->root->builder.CreatePointerCast(vtable.val, context->root->LookupType("char**")->GetLLVMType());

						auto myself = function->Load("this");

						std::vector<llvm::Value*> iindex = { context->root->builder.getInt32(0), context->root->builder.getInt32(i) };
						auto gep = context->root->builder.CreateGEP(myself.val, iindex, "getmember");
						context->root->builder.CreateStore(vtable.val, gep);
					}
					i++;
				}

				function->Return(CValue());

				context->root->builder.SetCurrentDebugLocation(dp);
				if (rp)
					context->root->builder.SetInsertPoint(rp);

				context->root->current_function = context;

				function->function->Load(context->root);
			}
		}
		else if (ii.second->name == "__" + str->data->name + "_" + strname)
		{
			auto rp = context->root->builder.GetInsertBlock();
			auto dp = context->root->builder.getCurrentDebugLocation();

			//setup vtable if there is one
			ii.second->Load(context->root);
			auto iter = ii.second->f->getBasicBlockList().begin()->begin();
			for (unsigned int i = 0; i < ii.second->arguments.size() * 3; i++)
				iter++;
			context->root->builder.SetInsertPoint(&*iter);

			//todo: oops code duplication here
			int i = 0;
			for (auto iip : str->data->struct_members)
			{
				if (iip.type->type == Types::Struct)
				{
					//call the constructor
					auto iiname = iip.type->data->template_base ? iip.type->data->template_base->name : iip.type->data->name;

					auto range = iip.type->data->functions.equal_range(iiname);
					for (auto iii = range.first; iii != range.second; iii++)
					{
						if (iii->second->arguments.size() == 1)
						{
							auto myself = ii.second->context->Load("this");
							std::vector<llvm::Value*> iindex = { context->root->builder.getInt32(0), context->root->builder.getInt32(i) };

							auto gep = context->root->builder.CreateGEP(myself.val, iindex, "getmember");
							iii->second->Load(context->root);

							context->root->builder.CreateCall(iii->second->f, gep);
						}
					}
				}
				else if (iip.name == "__vtable")//is vtable)
				{
					//first load the global
					auto oldns = context->root->ns;
					context->root->ns = str->data;
					CValue vtable = ii.second->context->GetVariable("__" + this->name.text + "_vtable");
					context->root->ns = oldns;

					//cast the global to a char**
					vtable.val = context->root->builder.CreatePointerCast(vtable.val, context->root->LookupType("char**")->GetLLVMType());

					auto myself = ii.second->context->Load("this");

					std::vector<llvm::Value*> iindex = { context->root->builder.getInt32(0), context->root->builder.getInt32(i) };
					auto gep = context->root->builder.CreateGEP(myself.val, iindex, "getmember");
					context->root->builder.CreateStore(vtable.val, gep);
				}
				i++;
			}

			context->root->builder.SetCurrentDebugLocation(dp);
			if (rp)
				context->root->builder.SetInsertPoint(rp);
		}
		else if (ii.second->name == "__" + str->data->name + "_~" + strname)
		{
			auto rp = context->root->builder.GetInsertBlock();
			auto dp = context->root->builder.getCurrentDebugLocation();

			auto iter = ii.second->f->getBasicBlockList().begin()->begin();
			for (unsigned int i = 0; i < ii.second->arguments.size() * 3; i++)
				iter++;
			context->root->builder.SetInsertPoint(&*iter);

			int i = 0;
			for (auto iip : str->data->struct_members)
			{
				if (iip.type->type == Types::Struct)
				{
					//call the destructor
					auto range = iip.type->data->functions.equal_range("~" + iip.type->data->name);
					for (auto iii = range.first; iii != range.second; iii++)
					{
						if (iii->second->arguments.size() == 1)
						{
							auto myself = ii.second->context->Load("this");
							std::vector<llvm::Value*> iindex = { context->root->builder.getInt32(0), context->root->builder.getInt32(i) };

							auto gep = context->root->builder.CreateGEP(myself.val, iindex, "getmember");
							iii->second->Load(context->root);

							context->root->builder.CreateCall(iii->second->f, gep);
						}
					}
				}
				i++;
			}

			context->root->builder.SetCurrentDebugLocation(dp);
			if (rp)
            {
				context->root->builder.SetInsertPoint(rp);
            }
		}
	}
}


void StructExpression::CompileDeclarations(CompilerContext* context)
{
	//build data about the struct
	std::string type_name;
	const auto& ns = context->root->ns->GetQualifiedName();
	if (ns.length() > 0)
    {
		type_name = ns + "::" + this->name.text;
    }
	else
    {
		type_name = this->name.text;
    }

	Type* str = new Type(type_name, Types::Struct, new Struct);
	if (this->token.type == TokenType::Class)
    {
		str->data->is_class = true;
    }

	context->root->ns->members.insert({ this->name.text, str });
	str->ns = context->root->ns;

	str->data->name = type_name;
	str->data->expression = this;
	if (this->base_type.text.length())
    {
		context->root->AdvanceTypeLookup(&str->data->parent_struct, this->base_type.text, &this->base_type);
    }
	if (this->templates)
	{
		str->data->templates.reserve(this->templates->size());
		for (auto& ii : *this->templates)
		{
			str->data->templates.push_back({ 0, ii.name.text });
			context->root->AdvanceTypeLookup(&str->data->templates.back().first, ii.type.text, &ii.type);

			context->root->AdvanceTypeLookup(&str->data->members.insert({ ii.name.text, Symbol((Type*)0) })->second.ty, ii.type.text, &ii.type);
		}
	}

	//register the templates as a type, so all the members end up with the same type
	int size = 0;
	for (auto ii : this->members)
	{
		if (ii.type == StructMember::VariableMember)
        {
			size++;
        }
	}
	str->data->struct_members.reserve(size);

	str->data->parent = context->root->ns;
	context->root->ns = str->data;

	for (auto& ii : this->members)
	{
		if (ii.type == StructMember::VariableMember)
		{
			//error if we already have the member
			for (auto mem : str->data->struct_members)
			{
				if (mem.name == ii.variable.name.text)
				{
					//todo: show where first defintion is
					context->root->Error("Duplicate member variable '" + ii.variable.name.text + "'", ii.variable.name);
				}
			}
			str->data->struct_members.push_back({ ii.variable.name.text, ii.variable.type.text, 0 });
			if (this->templates == 0)
            {
				context->root->AdvanceTypeLookup(&str->data->struct_members.back().type, ii.variable.type.text, &ii.variable.type);
            }
		}
		else if (ii.type == StructMember::DefinitionMember)
		{
			ii.definition->CompileDeclarations(context);
		}
		else
		{
			if (this->templates == 0)//todo need to get rid of this to fix things
			{
				ii.function->CompileDeclarations(context);
			}
			else
			{
				//this is just for templated structs for typechecking
				auto func = new Function(ii.function->GetFunctionNamePrefix(), false);
				context->root->typecheck = true;
				context->root->AdvanceTypeLookup(&func->return_type, ii.function->ret_type.text, &ii.function->ret_type);
				func->arguments.push_back({ 0, "this" });
				for (unsigned int i = 0; i < ii.function->args->size(); i++)
				{
					func->arguments.push_back({ 0, ii.function->args->at(i).name.text });
				}

				context->root->typecheck = false;
				str->data->functions.insert({ ii.function->GetName(), func });
			}
		}
	}
	context->root->ns = context->root->ns->parent;

	if (this->templates == 0 && context->root->compiling_includes == false)
    {
		this->AddConstructorDeclarations(str, context);
    }
}

