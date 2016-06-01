#include "Expressions.h"
#include "Compiler.h"
#include "Parser.h"
#include "Types/Function.h"
#include "DeclarationExpressions.h"

using namespace Jet;

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DerivedTypes.h>

Type* FunctionExpression::TypeCheck(CompilerContext* context)
{
	bool is_lambda = name.text.length() == 0;

	//need to change context
	CompilerContext* nc = new CompilerContext(context->root, context);// context->AddFunction(this->GetRealName(), ret, argsv, Struct.text.length() > 0 ? true : false, is_lambda);// , this->varargs);
	nc->function = new Function("um", is_lambda);
	
	if (this->name.text.length() == 0)
	{
		context->CurrentToken(&this->ret_type);
		nc->function->return_type = context->root->LookupType(this->ret_type.text, false);

		//ok return type, im a lambda
		std::vector<std::pair<Type*, std::string>> argsv;
		for (auto ii : *this->args)
			argsv.push_back({ context->root->LookupType(ii.type.text), ii.name.text });

		std::vector<Type*> args;
		for (auto ii : argsv)
			args.push_back(ii.first);

		auto ret = context->root->LookupType(this->ret_type.text, false);

		return context->root->LookupType("function<" + context->root->GetFunctionType(ret, args)->ToString() + ">");
	}

	if (auto str = dynamic_cast<StructExpression*>(this->parent))
	{
		auto typ = context->root->LookupType(str->GetName(), false)->GetPointerType()->GetPointerType();
		nc->TCRegisterLocal("this", typ);
	}
	else if (this->Struct.text.length())
	{
		auto typ = context->root->LookupType(this->Struct.text, false);// ->GetPointerType()->GetPointerType();
		nc->TCRegisterLocal("this", typ->GetPointerType()->GetPointerType());

		//check if im extending a trait, if so add any relevant templates

		//todo: lets not typecheck this yet, its being troublesome
		//the main issue is that the template parameters of the traits are invalid types
		//not sure how to resolve this
		return 0;
		//lets not typecheck these yet....
		if (typ->base->base->type == Types::Trait)
			context->root->ns = typ->trait;
		else
			context->root->ns = typ->data;
	}

	context->CurrentToken(&this->ret_type);
	nc->function->return_type = context->root->LookupType(this->ret_type.text, false);

	for (auto ii : *this->args)
		nc->TCRegisterLocal(ii.name.text, context->root->LookupType(ii.type.text)->GetPointerType());

	if (this->is_generator)
	{
		auto func = context->root->ns->GetFunction(this->GetRealName());
		auto str = func->return_type;
		nc->local_reg_callback = [&](const std::string& name, Type* ty)
		{
			str->data->struct_members.push_back({ name, ty->base->name, ty->base });
		};
	}

	this->block->TypeCheck(nc);

	if (this->ret_type.text == "void")
		;
	else if (nc->function->has_return == false && this->is_generator == false)
		context->root->Error("Function must return a value!", token);

	if (this->Struct.text.length())
		context->root->ns = context->root->ns->parent;

	nc->local_reg_callback = [&](const std::string& name, Type* ty){};

	//delete temporary things
	delete nc->function;
	delete nc;

	return 0;
}


unsigned int uuid = 5;
std::string FunctionExpression::GetRealName()
{
	std::string fname;
	if (name.text.length() > 0)
		fname = name.text;
	else
		fname = "_lambda_id_" + std::to_string(uuid++);
	auto Struct = dynamic_cast<StructExpression*>(this->parent);
	if (this->Struct.text.length() > 0)
		return "__" + this->Struct.text + "_" + fname;
	else
		return Struct ? "__" + Struct->GetName() + "_" + fname : fname;
}

CValue FunctionExpression::Compile(CompilerContext* context)
{
	auto Struct = dynamic_cast<StructExpression*>(this->parent) ? dynamic_cast<StructExpression*>(this->parent)->GetName() : this->Struct.text;

	context->current_token = &this->token;
	//need to not compile if template or trait
	if (this->templates)
	{
		for (auto ii : *this->templates)//make sure traits are valid
		{
			auto iter = context->root->traits.find(ii.first.text);
			if (iter == context->root->traits.end() || iter->second->valid == false)
				context->root->Error("Trait '" + ii.first.text + "' is not defined", ii.first);
		}
		return CValue();
	}

	if (Struct.length())
	{
		auto iter = context->root->LookupType(Struct, false);
		if (iter->type == Types::Trait)
			return CValue();
	}

	return this->DoCompile(context);
}

#include <llvm\IR\BasicBlock.h>
CValue FunctionExpression::DoCompile(CompilerContext* context)
{
	context->CurrentToken(&token);

	bool is_lambda = name.text.length() == 0;

	//build list of types of vars
	std::vector<std::pair<Type*, std::string>> argsv;
	auto Struct = dynamic_cast<StructExpression*>(this->parent) ? dynamic_cast<StructExpression*>(this->parent)->GetName() : this->Struct.text;

	//insert this argument if I am a member function
	if (Struct.length() > 0)
	{
		auto type = context->root->LookupType(Struct + "*");

		argsv.push_back({ type, "this" });
	}

	llvm::BasicBlock* yieldbb;//location of starting point in generator function
	if (this->is_generator)
	{
		//add the context pointer as an argument if this is a generator
		auto func = context->root->ns->GetFunction(this->GetRealName());

		auto str = func->return_type;
		str->Load(context->root);
		argsv.push_back({ str->GetPointerType(), "_context" });
	}

	auto rp = context->root->builder.GetInsertBlock();
	auto dp = context->root->builder.getCurrentDebugLocation();
	if (is_lambda)
	{
		//get parent
		auto call = dynamic_cast<CallExpression*>(this->parent);

		//find my type
		if (call)
		{
			//look for me in the args
			if (call->left == this)
				context->root->Error("Cannot imply type of lambda with the args of its call", *context->current_token);

			int i = dynamic_cast<NameExpression*>(call->left) ? 0 : 1;
			for (; i < call->args->size(); i++)
			{
				if ((*call->args)[i].first == this)
					break;
			}
			//todo: ok, this is the wrong way to get the function arg type needed
			CValue fun = call->left->Compile(context);
			Type* type = fun.type->function->args[i];

			if (type->type == Types::Function)
			{
				//do type inference
				int i2 = 0;
				for (auto& ii : *this->args)
				{
					if (ii.type.text.length() == 0)//do type inference
						ii.type.text = type->function->args[i2]->ToString();
					i2++;
				}

				if (this->ret_type.text.length() == 0)//infer return type
					this->ret_type.text = type->function->return_type->ToString();
			}
			else if (type->type == Types::Struct && type->data->template_base->name == "function")
			{
				//do type inference
				auto fun = type->data->template_args[0]->function;
				int i2 = 0;
				for (auto& ii : *this->args)
				{
					if (ii.type.text.length() == 0)//do type inference
						ii.type.text = fun->args[i2]->ToString();
					i2++;
				}

				if (this->ret_type.text.length() == 0)//infer return type
					this->ret_type.text = fun->return_type->ToString();
			}
		}
		else
		{
			for (auto ii : *this->args)
				if (ii.type.text.length() == 0)
					context->root->Error("Lambda type inference only implemented for function calls", *context->current_token);
			if (this->ret_type.text.length() == 0)
				context->root->Error("Lambda type inference only implemented for function calls", *context->current_token);
		}
	}

	for (auto ii : *this->args)
		argsv.push_back({ context->root->LookupType(ii.type.text), ii.name.text });

	context->CurrentToken(&this->ret_type);
	Type* ret;
	if (this->is_generator)
		ret = context->root->LookupType("bool");
	else
		ret = context->root->LookupType(this->ret_type.text);

	llvm::Value* lambda = 0;

	Type* lambda_type = 0;
	llvm::StructType* storage_t = 0;
	if (is_lambda)
	{
		//allocate the function object
		std::vector<Type*> args;
		for (auto ii : argsv)
			args.push_back(ii.first);

		lambda_type = context->root->LookupType("function<" + context->root->GetFunctionType(ret, args)->ToString() + ">");
		lambda = context->root->builder.CreateAlloca(lambda_type->GetLLVMType());

		storage_t = llvm::StructType::get(context->root->context, {});

		argsv.push_back({ context->root->LookupType("char*"), "_capture_data" });
	}

	CompilerContext* function;
	if (this->is_generator)
	{
		function = context->AddFunction(this->GetRealName() + "_generator", ret, { argsv.front() }, Struct.length() > 0 ? true : false, is_lambda);// , this->varargs);
		function->function->is_generator = true;
	}
	else
		function = context->AddFunction(this->GetRealName(), ret, argsv, Struct.length() > 0 ? true : false, is_lambda);// , this->varargs);

	function->function->lambda.storage_type = storage_t;
	context->root->current_function = function;
	function->function->Load(context->root);

	function->SetDebugLocation(this->token);

	if (is_lambda)
		function->function->do_export = false;

	//alloc args
	auto AI = function->function->f->arg_begin();
	for (unsigned Idx = 0, e = argsv.size(); Idx != e; ++Idx, ++AI)
	{
		// Create an alloca for this variable.
		if (!(Idx > 0 && this->is_generator))
		{
			auto aname = argsv[Idx].second;

			llvm::IRBuilder<> TmpB(&function->function->f->getEntryBlock(), function->function->f->getEntryBlock().begin());
			auto Alloca = TmpB.CreateAlloca(argsv[Idx].first->GetLLVMType(), 0, aname);
			// Store the initial value into the alloca.
			function->root->builder.CreateStore(AI, Alloca);

			AI->setName(aname);

			auto D = context->root->debug->createLocalVariable(llvm::dwarf::DW_TAG_arg_variable, function->function->scope, aname, context->root->debug_info.file, this->token.line,
				argsv[Idx].first->GetDebugType(context->root));

			llvm::Instruction *Call = context->root->debug->insertDeclare(
				Alloca, D, context->root->debug->createExpression(), llvm::DebugLoc::get(this->token.line, this->token.column, function->function->scope), Alloca);
			Call->setDebugLoc(llvm::DebugLoc::get(this->token.line, this->token.column, function->function->scope));

			// Add arguments to variable symbol table.
			function->RegisterLocal(aname, CValue(argsv[Idx].first->GetPointerType(), Alloca));
		}
	}

	if (this->is_generator)
	{
		//compile the start code for a generator function
		auto data = function->Load("_context");

		//add arguments
		int i = 0;
		for (auto ii : *this->args)
		{
			auto ptr = data.val;
			auto val = function->root->builder.CreateGEP(ptr, { function->root->builder.getInt32(0), function->root->builder.getInt32(2 + i++) });
			function->RegisterLocal(ii.name.text, CValue(function->root->LookupType(ii.type.text)->GetPointerType(), val));
		}

		//add local vars
		for (int i = 2 + this->args->size(); i < data.type->base->data->struct_members.size(); i++)
		{
			auto gep = context->root->builder.CreateGEP(data.val, { context->root->builder.getInt32(0), context->root->builder.getInt32(i) });
			function->function->generator.variable_geps.push_back(gep);
		}

		//branch to the continue point
		auto br = function->root->builder.CreateGEP(data.val, { context->root->builder.getInt32(0), context->root->builder.getInt32(0) });
		auto loc = function->root->builder.CreateLoad(br);
		auto ibr = function->root->builder.CreateIndirectBr(loc, 10);
		function->function->generator.ibr = ibr;

		//add new bb
		yieldbb = llvm::BasicBlock::Create(llvm::getGlobalContext(), "yield", function->function->f);
		ibr->addDestination(yieldbb);

		function->root->builder.SetInsertPoint(yieldbb);
	}

	block->Compile(function);

	//check for return, and insert one or error if there isnt one
	if (function->function->is_generator)
		function->root->builder.CreateRet(function->root->builder.getInt1(false));//signal we are gone generating values
	else if (function->function->f->getBasicBlockList().back().getTerminator() == 0)
		if (this->ret_type.text == "void")
			function->Return(CValue());
		else
			context->root->Error("Function must return a value!", token);

	//remove instructions after the first terminator in a block
	for (auto ii = function->function->f->getBasicBlockList().begin(); ii != function->function->f->getBasicBlockList().end(); ii++)
	{
		bool returned = false;
		for (auto inst = ii->begin(); inst != ii->end(); )
		{
			if (returned == true)
			{
				auto temp = inst.getNodePtrUnchecked();
				inst++;
				temp->eraseFromParent();
			}
			else if (inst->isTerminator())
			{
				returned = true;
				inst++;
			}
			else
			{
				inst++;
			}
		}
	}
	
	//fix having instructions after a return in a block
	//so need to check every basic block for anything after a return
	if (this->is_generator)
	{
		//compile the other function necessary for an iterator
		auto func = context->AddFunction(this->GetRealName(), 0, { argsv.front() }, Struct.length() > 0 ? true : false, is_lambda);

		auto str = func->function->return_type;

		context->root->current_function = func;
		func->function->Load(context->root);

		//alloca the new context
		auto alloc = context->root->builder.CreateAlloca(str->GetLLVMType());

		//set the branch location to the start
		auto ptr = context->root->builder.CreateGEP(alloc, { context->root->builder.getInt32(0), context->root->builder.getInt32(0) });
		auto val = llvm::BlockAddress::get(yieldbb);
		context->root->builder.CreateStore(val, ptr);

		//store arguments into context
		if (argsv.size() > 1)
		{
			auto AI = func->function->f->arg_begin();
			for (int i = 1; i < argsv.size(); i++)
			{
				auto ptr = context->root->builder.CreateGEP(alloc, { context->root->builder.getInt32(0), context->root->builder.getInt32(2 + i - 1) });
				context->root->builder.CreateStore(AI++, ptr);
			}
		}

		//then return the newly created iterator object
		context->root->builder.CreateRet(context->root->builder.CreateLoad(alloc));

		//now compile reset function
		{
			auto reset = context->AddFunction(this->GetRealName() + "yield_reset", context->root->LookupType("void"), { argsv.front() }, Struct.length() > 0 ? true : false, is_lambda);// , this->varargs);

			context->root->current_function = reset;
			reset->function->Load(context->root);

			auto self = reset->function->f->arg_begin();
			//set the branch location back to start
			auto ptr = context->root->builder.CreateGEP(self, { context->root->builder.getInt32(0), context->root->builder.getInt32(0) });
			auto val = llvm::BlockAddress::get(yieldbb);
			context->root->builder.CreateStore(val, ptr);

			context->root->builder.CreateRetVoid();

			auto& x = str->data->functions.find("Reset");
			x->second = reset->function;
		}

		//compile current function
		{
			auto current = context->AddFunction(this->GetRealName() + "generator_current", context->root->LookupType(this->ret_type.text), { argsv.front() }, Struct.length() > 0 ? true : false, is_lambda);// , this->varargs);

			//add a return and shizzle
			context->root->current_function = current;
			current->function->Load(context->root);

			//return the current value
			auto self = current->function->f->arg_begin();
			auto ptr = context->root->builder.CreateGEP(self, { context->root->builder.getInt32(0), context->root->builder.getInt32(1) });
			context->root->builder.CreateRet(context->root->builder.CreateLoad(ptr));

			auto& x = str->data->functions.find("Current");
			x->second = current->function;
		}

		//Set the generator function as MoveNext
		auto& x = str->data->functions.find("MoveNext");
		x->second = function->function;
	}

	//reset insertion point to where it was before (for lambdas and template compilation)
	context->root->builder.SetCurrentDebugLocation(dp);
	if (rp)
		context->root->builder.SetInsertPoint(rp);
	context->root->current_function = context;

	//store the lambda value
	if (lambda)
	{
		function->WriteCaptures(lambda);

		auto ptr = context->root->builder.CreateGEP(lambda, { context->root->builder.getInt32(0), context->root->builder.getInt32(0) }, "name");

		ptr = context->root->builder.CreatePointerCast(ptr, function->function->f->getType()->getPointerTo());
		context->root->builder.CreateStore(function->function->f, ptr);
	}

	context->CurrentToken(&this->token);
	if (lambda)//return the lambda if we are one
		return CValue(lambda_type, context->root->builder.CreateLoad(lambda));
	else
		return CValue(function->function->GetType(context->root), function->function->f);
}

void FunctionExpression::CompileDeclarations(CompilerContext* context)
{
	context->CurrentToken(&this->token);
	std::string fname = name.text;

	if (name.text.length() == 0)
		return;//dont compile expression for lambdas

	bool advlookup = true;
	Function* fun = new Function(this->GetRealName(), false);
	fun->expression = this;
	context->root->functions.push_back(fun);
	auto str = dynamic_cast<StructExpression*>(this->parent) ? dynamic_cast<StructExpression*>(this->parent)->GetName() : this->Struct.text;

	fun->f = 0;
	bool is_trait = false;
	if (str.length() > 0)
	{
		auto type = context->root->LookupType(str, false);
		if (type->type == Types::Trait)
		{
			type->Load(context->root);
			type->trait->extension_methods.insert({ fname, fun });
			is_trait = true;
		}
		else
		{
			type->data->functions.insert({ fname, fun });
			advlookup = !type->data->template_args.size();
		}
	}
	else
		context->root->ns->members.insert({ fname, fun });


	if (is_generator)
	{
		//build data about the generator context struct
		Type* str = new Type;
		str->name = this->GetRealName() + "_yielder_context";
		str->type = Types::Struct;
		str->data = new Jet::Struct;
		str->data->name = str->name;
		str->data->parent_struct = 0;
		context->root->ns->members.insert({ str->name, str });


		//add default iterator methods, will fill in function later
		str->data->functions.insert({ "MoveNext", 0 });
		str->data->functions.insert({ "Reset", 0 });
		str->data->functions.insert({ "Current", 0 });

		//add the position and return variables to the context
		str->data->struct_members.push_back({ "position", "char*", context->root->LookupType("char*") });
		str->data->struct_members.push_back({ "return", this->ret_type.text, context->root->LookupType(this->ret_type.text) });

		//add arguments to context
		for (auto ii : *this->args)
			str->data->struct_members.push_back({ ii.name.text, ii.type.text, context->root->LookupType(ii.type.text) });

		str->ns = context->root->ns;

		//DO NOT LOAD THIS UNTIL COMPILATION, IN FACT DONT LOAD ANYTHING
		//str->Load(context->root);
		fun->return_type = str;
	}
	else if (is_trait)
		fun->return_type = 0;
	else if (advlookup)
		context->root->AdvanceTypeLookup(&fun->return_type, this->ret_type.text, &this->ret_type);
	else
		fun->return_type = context->root->LookupType(this->ret_type.text, false);

	fun->arguments.reserve(this->args->size() + (str.length() ? 1 : 0));

	//add the this pointer argument if this is a member function
	if (str.length() > 0)
	{
		fun->arguments.push_back({ 0, "this" });
		if (is_trait == false && advlookup)
			context->root->AdvanceTypeLookup(&fun->arguments.back().first, str + "*", &this->token);
		else if (is_trait == false)
			fun->arguments.back().first = context->root->LookupType(str + "*", false);
	}

	//add arguments to new function
	for (auto ii : *this->args)
	{
		Type* type = 0;
		if (!advlookup)//else
			type = context->root->LookupType(ii.type.text);

		fun->arguments.push_back({ type, ii.name.text });
		if (advlookup && is_trait == false)
			context->root->AdvanceTypeLookup(&fun->arguments.back().first, ii.type.text, &this->token);
	}

	//add templates to new function
	if (this->templates)
	{
		for (auto ii : *this->templates)
			fun->templates.push_back({ context->root->LookupType(ii.first.text), ii.second.text });
	}
}

CValue ExternExpression::Compile(CompilerContext* context)
{
	return CValue();
}

void ExternExpression::CompileDeclarations(CompilerContext* context)
{
	std::string fname = name.text;

	Function* fun = new Function(fname, false);

	if (auto attr = dynamic_cast<AttributeExpression*>(this->parent))
	{
		//add the attribute to the fun here
		if (attr->name.text == "stdcall")
			fun->calling_convention = CallingConvention::StdCall;
		else if (attr->name.text == "thiscall")
			fun->calling_convention = CallingConvention::ThisCall;
		else if (attr->name.text == "fastcall")
			fun->calling_convention = CallingConvention::FastCall;
		//add the attribute to the fun
	}
	context->root->AdvanceTypeLookup(&fun->return_type, this->ret_type.text, &this->ret_type);

	fun->arguments.reserve(this->args->size() + (Struct.length() > 0 ? 1 : 0));
	

	fun->f = 0;
	if (Struct.length() > 0)
	{
		fun->name = "__" + Struct + "_" + fname;//mangled name

		//add to struct
		auto ii = context->root->TryLookupType(Struct);
		if (ii == 0)//its new
		{
			context->root->Error("Not implemented!", token);
			//str = new Type;
			//context->root->types[this->name] = str;
		}
		else
		{
			if (ii->type != Types::Struct)
				context->root->Error("Cannot define an extern function for a type that is not a struct", token);

			ii->data->functions.insert({ fname, fun });
		}

		fun->arguments.push_back({ ii->GetPointerType(), "this" });
	}
	else
	{
		context->root->ns->members.insert({ fname, fun });
	}

	//look up arg types
	for (auto ii : *this->args)
	{
		fun->arguments.push_back({ 0, ii.name.text });
		context->root->AdvanceTypeLookup(&fun->arguments.back().first, ii.type.text, &this->token);
	}
}

CValue LocalExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&(*_names)[0].second);

	if (this->parent->parent == 0)
	{
		//im in global scope
		auto type = context->root->LookupType(this->_names->front().first.text);
		auto global = context->root->AddGlobal(this->_names->front().second.text, type);

		//should I add a constructor?
		if (this->_right && this->_right->size() > 0)
			context->root->Error("Initializing global variables not yet implemented", token);

		return CValue();
	}

	int i = 0;
	for (auto ii : *this->_names) {
		auto aname = ii.second.text;

		Type* type = 0;
		llvm::AllocaInst* Alloca = 0;
		if (ii.first.text.length() > 0)//type was specified
		{
			context->CurrentToken(&ii.first);
			type = context->root->LookupType(ii.first.text);

			if (type->type == Types::Struct && type->data->templates.size() > 0)
				context->root->Error("Missing template arguments for type '" + type->ToString() + "'", ii.first);
			else if (type->type == Types::Array)
			{
				Alloca = context->root->builder.CreateAlloca(type->GetLLVMType(), context->root->builder.getInt32(type->size), aname);
				//Alloca->dump();
				//Alloca->getType()->dump();

				//cast it to a pointer

			}
			else
			{
				if (type->GetBaseType()->type == Types::Trait)
					context->root->Error("Cannot instantiate trait", ii.second);

				Alloca = context->root->builder.CreateAlloca(type->GetLLVMType(), 0, aname);
			}

			auto D = context->root->debug->createLocalVariable(llvm::dwarf::DW_TAG_auto_variable, context->function->scope, aname, context->root->debug_info.file, ii.second.line,
				type->GetDebugType(context->root));

			llvm::Instruction *Call = context->root->debug->insertDeclare(
				Alloca, D, context->root->debug->createExpression(), llvm::DebugLoc::get(this->token.line, this->token.column, context->function->scope), Alloca);
			Call->setDebugLoc(llvm::DebugLoc::get(ii.second.line, ii.second.column, context->function->scope));

			// Store the initial value into the alloca.
			if (this->_right)
			{
				auto val = (*this->_right)[i++].second->Compile(context);
				//cast it
				val = context->DoCast(type, val);
				context->root->builder.CreateStore(val.val, Alloca);
			}
		}
		else if (this->_right)
		{
			//infer the type
			auto val = (*this->_right)[i++].second->Compile(context);
			type = val.type;

			if (val.type->type == Types::Array)
				Alloca = context->root->builder.CreateAlloca(val.type->GetLLVMType(), context->root->builder.getInt32(val.type->size), aname);
			else
				Alloca = context->root->builder.CreateAlloca(val.type->GetLLVMType(), 0, aname);

			llvm::DIFile* unit = context->root->debug_info.file;
			type->Load(context->root);
			llvm::DILocalVariable* D = context->root->debug->createLocalVariable(llvm::dwarf::DW_TAG_arg_variable, context->function->scope, aname, unit, ii.second.line,
				type->GetDebugType(context->root));

			llvm::Instruction *Call = context->root->debug->insertDeclare(
				Alloca, D, context->root->debug->createExpression(), llvm::DebugLoc::get(this->token.line, this->token.column, context->function->scope), Alloca);
			Call->setDebugLoc(llvm::DebugLoc::get(ii.second.line, ii.second.column, context->function->scope));

			context->root->builder.CreateStore(val.val, Alloca);
		}
		else
			context->root->Error("Cannot infer type from nothing!", ii.second);

		// Add arguments to variable symbol table.
		if (context->function->is_generator)
		{
			//find the already added type with the same name
			auto ty = context->function->arguments[0].first->base;
			/*std::vector<llvm::Type*> types;
			for (int i = 0; i < ty->data->type->getStructNumElements(); i++)
			types.push_back(((llvm::StructType*)ty->data->type)->getElementType(i));
			((llvm::StructType*)ty->data->type)->setBody(types);
			ty->data->struct_members.push_back({ aname, "type", type });*/
			//ok, almost this doesnt quite work right
			auto var_ptr = context->function->generator.variable_geps[context->function->generator.var_num++];
			
			if (this->_right)
			{
				auto val = (*this->_right)[i - 1].second->Compile(context);
				val = context->DoCast(type, val);

				context->root->builder.CreateStore(val.val, var_ptr);
			}

			//still need to do store
			context->RegisterLocal(aname, CValue(type->GetPointerType(), var_ptr));
		}
		else
			context->RegisterLocal(aname, CValue(type->GetPointerType(), Alloca));

		//construct it!
		if (this->_right == 0)
		{
			if (type->type == Types::Struct)
				context->Construct(CValue(type->GetPointerType(), Alloca), 0);
				
			if (type->type == Types::Array && type->base->type == Types::Struct)
				context->Construct(CValue(type, Alloca), context->root->builder.getInt32(type->size));
		}
	}

	return CValue();
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

void StructExpression::AddConstructorDeclarations(Type* str, CompilerContext* context)
{
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
	if (has_constructor == false)
	{
		auto fun = new Function("__" + str->data->name + "_" + strname, false);//
		fun->return_type = &VoidType;
		fun->arguments = { { context->root->LookupType(str->data->name + "*", false), "this" } };
		fun->f = 0;
		fun->expression = 0;
		fun->type = (FunctionType*)-1;
		str->data->functions.insert({ strname, fun });//register function in _Struct
	}
	if (has_destructor == false)
	{
		auto fun = new Function("__" + str->data->name + "_~" + strname, false);//
		fun->return_type = &VoidType;
		fun->arguments = { { context->root->LookupType(str->data->name + "*", false), "this" } };
		fun->f = 0;
		fun->expression = 0;
		fun->type = (FunctionType*)-1;

		str->data->functions.insert({ "~" + strname, fun });
	}
}

void StructExpression::AddConstructors(CompilerContext* context)
{
	auto Struct = this->GetName();

	Type* str = context->root->LookupType(Struct);
	std::string strname = str->data->template_base ? str->data->template_base->name : str->data->name;
	//fix this we are sometimes getting too many destructors added
	for (auto ii : str->data->functions)
	{
		//need to identify if its a autogenerated fun
		if (ii.second->expression == 0 && ii.second->type == (FunctionType*)-1 && ii.second->name.length() > strname.length() + 2 && ii.second->name.substr(2, strname.length()) == strname)
		{
			//its probably a constructor/destructor we need to fill in
			auto res = ii.second->name.find('~');
			if (res != -1)
			{
				//destructor
				auto rp = context->root->builder.GetInsertBlock();
				auto dp = context->root->builder.getCurrentDebugLocation();

				std::vector<std::pair<Type*, std::string>> argsv;
				argsv.push_back({ context->root->LookupType(str->data->name + "*"), "this" });

				//context->CurrentToken(&this->ret_type);
				auto ret = context->root->LookupType("void");

				CompilerContext* function = context->AddFunction(ii.second->name, ret, argsv, Struct.length() > 0 ? true : false, false);// , this->varargs);
				ii.second->f = function->function->f;

				context->root->current_function = function;

				function->function->context = function;
				function->function->Load(context->root);
				function->SetDebugLocation(this->token);

				//alloc args
				auto AI = function->function->f->arg_begin();
				for (unsigned Idx = 0, e = argsv.size(); Idx != e; ++Idx, ++AI) {
					// Create an alloca for this variable.
					auto aname = argsv[Idx].second;

					llvm::IRBuilder<> TmpB(&function->function->f->getEntryBlock(), function->function->f->getEntryBlock().begin());
					auto Alloca = TmpB.CreateAlloca(argsv[Idx].first->GetLLVMType(), 0, aname);
					// Store the initial value into the alloca.
					function->root->builder.CreateStore(AI, Alloca);

					AI->setName(aname);

					auto D = context->root->debug->createLocalVariable(llvm::dwarf::DW_TAG_arg_variable, function->function->scope, aname, context->root->debug_info.file, this->token.line,
						argsv[Idx].first->GetDebugType(context->root));

					llvm::Instruction *Call = context->root->debug->insertDeclare(
						Alloca, D, context->root->debug->createExpression(), llvm::DebugLoc::get(this->token.line, this->token.column, function->function->scope), Alloca);
					Call->setDebugLoc(llvm::DebugLoc::get(this->token.line, this->token.column, function->function->scope));

					// Add arguments to variable symbol table.
					function->RegisterLocal(aname, CValue(argsv[Idx].first->GetPointerType(), Alloca));
				}

				//compile stuff here
				int i = 0;
				for (auto ii : str->data->struct_members)
				{
					if (ii.type->type == Types::Struct)
					{
						//call the constructor
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

				//check for return, and insert one or error if there isnt one
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
				argsv.push_back({ context->root->LookupType(str->data->name + "*"), "this" });

				auto ret = context->root->LookupType("void");

				CompilerContext* function = context->AddFunction(ii.second->name, ret, argsv, Struct.length() > 0 ? true : false, false);// , this->varargs);
				ii.second->f = function->function->f;

				context->root->current_function = function;

				function->function->context = function;
				function->function->Load(context->root);
				function->SetDebugLocation(this->token);

				//alloc args
				auto AI = function->function->f->arg_begin();
				for (unsigned Idx = 0, e = argsv.size(); Idx != e; ++Idx, ++AI) {
					// Create an alloca for this variable.
					auto aname = argsv[Idx].second;

					llvm::IRBuilder<> TmpB(&function->function->f->getEntryBlock(), function->function->f->getEntryBlock().begin());
					auto Alloca = TmpB.CreateAlloca(argsv[Idx].first->GetLLVMType(), 0, aname);
					// Store the initial value into the alloca.
					function->root->builder.CreateStore(AI, Alloca);

					AI->setName(aname);

					llvm::DIFile* unit = context->root->debug_info.file;

					auto D = context->root->debug->createLocalVariable(llvm::dwarf::DW_TAG_arg_variable, function->function->scope, aname, unit, this->token.line,
						argsv[Idx].first->GetDebugType(context->root));

					llvm::Instruction *Call = context->root->debug->insertDeclare(
						Alloca, D, context->root->debug->createExpression(), llvm::DebugLoc::get(this->token.line, this->token.column, function->function->scope), Alloca);
					Call->setDebugLoc(llvm::DebugLoc::get(this->token.line, this->token.column, function->function->scope));

					// Add arguments to variable symbol table.
					function->RegisterLocal(aname, CValue(argsv[Idx].first->GetPointerType(), Alloca));
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
					i++;
				}

				//check for return, and insert one or error if there isnt one
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

			auto iter = ii.second->f->getBasicBlockList().begin()->begin();
			for (int i = 0; i < ii.second->arguments.size() * 3; i++)
				iter++;
			context->root->builder.SetInsertPoint(iter);

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
			for (int i = 0; i < ii.second->arguments.size() * 3; i++)
				iter++;
			context->root->builder.SetInsertPoint(iter);

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
				context->root->builder.SetInsertPoint(rp);
		}
	}
}


void StructExpression::CompileDeclarations(CompilerContext* context)
{
	//build data about the struct
	Type* str = new Type(this->name.text, Types::Struct, new Struct);
	context->root->ns->members.insert({ this->name.text, str });

	str->data->name = this->name.text;
	str->data->expression = this;
	if (this->base_type.text.length())
		context->root->AdvanceTypeLookup(&str->data->parent_struct, this->base_type.text, &this->base_type);
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
			size++;
	}
	str->data->struct_members.reserve(size);

	str->data->parent = context->root->ns;
	context->root->ns = str->data;
	for (auto& ii : this->members)
	{
		if (ii.type == StructMember::VariableMember)
		{
			str->data->struct_members.push_back({ ii.variable.name.text, ii.variable.type.text, 0 });
			if (this->templates == 0)
				context->root->AdvanceTypeLookup(&str->data->struct_members.back().type, ii.variable.type.text, &ii.variable.type);
		}
		else
		{
			if (this->templates == 0)//todo need to get rid of this to fix things
				ii.function->CompileDeclarations(context);
			else
			{
				//this is just for templated structs for typechecking
				auto func = new Function(ii.function->GetRealName(), false);
				context->root->typecheck = true;
				context->root->AdvanceTypeLookup(&func->return_type, ii.function->ret_type.text, &ii.function->ret_type);
				func->arguments.push_back({ 0, "this" });
				for (int i = 0; i < ii.function->args->size(); i++)
					func->arguments.push_back({ 0, ii.function->args->at(i).name.text });
				
				context->root->typecheck = false;
				str->data->functions.insert({ ii.function->GetName(), func});
			}
		}
	}
	context->root->ns = context->root->ns->parent;

	if (this->templates == 0)
		this->AddConstructorDeclarations(str, context);
}

void TraitExpression::CompileDeclarations(CompilerContext* context)
{
	//check if trait already exists, if its in the table and set as invalid, then we can just fill in the blanks
	Trait* t;
	auto tr = context->root->ns->members.find(name.text);
	if (tr == context->root->ns->members.end())
	{
		t = new Trait;
		Type* ty = new Type(name.text, Types::Trait);
		ty->trait = t;
		context->root->ns->members.insert({ name.text, Symbol(ty) });
		context->root->traits[name.text] = t;
	}
	else
		context->root->Error("Type '" + name.text + "' already exists", token);
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

			auto type = new Type;
			type->name = ii.name.text;
			type->type = Types::Invalid;
			type->ns = context->root->ns;
			t->members.insert({ ii.name.text, type });
		}
	}

	for (auto ii : this->funcs)
	{
		Function* func = new Function(ii.name.text, false);
		context->root->AdvanceTypeLookup(&func->return_type, ii.ret_type.text, &ii.ret_type);
		func->arguments.reserve(ii.args.size());
		for (auto arg : ii.args)
		{
			func->arguments.push_back({ 0, "dummy" });
			context->root->AdvanceTypeLookup(&func->arguments.back().first, arg.type.text, &arg.type);
		}

		t->functions.insert({ ii.name.text, func });
	}

	context->root->ns = t->parent;
}
