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
	if (this->captures)
		is_lambda = false;
	//if we have specifier we are not lambda, just inline function

	//need to change context
	CompilerContext* nc = new CompilerContext(context->root, context);
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

		if (is_lambda == false)
			return context->root->GetFunctionType(ret, args);
		else
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
	if (this->templates == 0)
	{
		nc->function->return_type = context->root->LookupType(this->ret_type.text, false);

		for (auto ii : *this->args)
			nc->TCRegisterLocal(ii.name.text, context->root->LookupType(ii.type.text)->GetPointerType());
	}

	if (this->is_generator)
	{
		auto func = context->root->ns->GetFunction(this->GetRealName());
		auto str = func->return_type;
		//add _context
		nc->TCRegisterLocal("_context", str->GetPointerType()->GetPointerType());

		nc->local_reg_callback = [&](const std::string& name, Type* ty)
		{
			str->data->struct_members.push_back({ name, ty->base->name, ty->base });
		};

		this->block->TypeCheck(nc);

		if (this->ret_type.text == "void")
			;
		else if (nc->function->has_return == false && this->is_generator == false)
			context->root->Error("Function must return a value!", token);
	}

	if (this->Struct.text.length())
		context->root->ns = context->root->ns->parent;

	nc->local_reg_callback = [&](const std::string& name, Type* ty) {};

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
		fname = "_lambda_id_" + std::to_string(uuid++);// Todo fix this returning a different value each time
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

#include <llvm/IR/BasicBlock.h>
CValue FunctionExpression::DoCompile(CompilerContext* context)
{
	context->CurrentToken(&token);

	bool is_lambda = name.text.length() == 0;
	if (this->captures)
		is_lambda = false;//todo this seems wrong...

	//if we have specifier we are not lambda, just inline function

	//build list of types of vars
	std::vector<std::pair<Type*, std::string>> argsv;
	auto struct_name = dynamic_cast<StructExpression*>(this->parent) ? dynamic_cast<StructExpression*>(this->parent)->GetName() : this->Struct.text;

	//insert the 'this' argument if I am a member function
	if (struct_name.length() > 0)
	{
		auto type = context->root->LookupType(struct_name + "*");

		argsv.push_back({ type, "this" });
	}

	//add the context pointer as an argument if this is a generator
	if (this->is_generator)
	{
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

			unsigned int i = dynamic_cast<NameExpression*>(call->left) ? 0 : 1;
			for (; i < call->args->size(); i++)
			{
				if ((*call->args)[i].first == this)
					break;
			}
			//todo: ok, this is the wrong way to get the function arg type needed
			//CValue fun = call->left->Compile(context);
			Type* type = call->left->TypeCheck(context); //fun.type->function->args[i];

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

	// Determine return type
	Type* ret;
	if (this->is_generator)
		ret = context->root->BoolType;
	else
		ret = context->root->LookupType(this->ret_type.text);

	// Build lambda data if we are one
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

		argsv.push_back({ context->root->CharPointerType, "_capture_data" });
	}

	// Get or create the relevant function CompilerContext
	CompilerContext* function_context;
	if (this->is_generator)
	{
		function_context = argsv.front().first->base->data->functions.find("MoveNext")->second->context;
		function_context->function->Load(context->root);
		llvm::BasicBlock *bb = llvm::BasicBlock::Create(context->root->context, "entry", function_context->function->f);
		context->root->builder.SetInsertPoint(bb);

		function_context->function->is_generator = true;
	}
	else
	{
		function_context = context->AddFunction(this->GetRealName(), ret, argsv, struct_name.length() > 0 ? argsv[0].first->base : 0, is_lambda);// , this->varargs);
	}

	function_context->function->lambda.storage_type = storage_t;
	context->root->current_function = function_context;
	function_context->function->Load(context->root);

	function_context->SetDebugLocation(this->token);

	//alloc args
	auto AI = function_context->function->f->arg_begin();
	if (function_context->function->return_type->type == Types::Struct)
	{
		AI++;
	}
	for (unsigned i = 0, e = argsv.size(); i != e; ++i, ++AI)
	{
		// Create an alloca for this variable.
		if (i > 0 && this->is_generator)
			continue;

		auto arg_name = argsv[i].second;

		llvm::IRBuilder<> TmpB(&function_context->function->f->getEntryBlock(), function_context->function->f->getEntryBlock().begin());
		//need to alloca pointer to struct if this is a struct type
		llvm::AllocaInst* Alloca = TmpB.CreateAlloca(argsv[i].first->GetLLVMType(), 0, arg_name);
		llvm::Value* storeval = AI;
		if (argsv[i].first->type == Types::Struct)
			storeval = function_context->root->builder.CreateLoad(AI);

		// Store the initial value into the alloca.
		function_context->root->builder.CreateStore(storeval, Alloca);

		AI->setName(arg_name);

		//insert debug declarations
		auto local = context->root->debug->createAutoVariable(function_context->function->scope, arg_name,
			context->root->debug_info.file, this->token.line,
			argsv[i].first->GetDebugType(context->root));

		llvm::Instruction* call = context->root->debug->insertDeclare(Alloca, local, context->root->debug->createExpression(),
			llvm::DebugLoc::get(this->token.line, this->token.column, function_context->function->scope),
			context->root->builder.GetInsertBlock());
		call->setDebugLoc(llvm::DebugLoc::get(this->token.line, this->token.column, function_context->function->scope));

		// Add arguments to variable symbol table.
		function_context->RegisterLocal(arg_name, CValue(argsv[i].first->GetPointerType(), Alloca));
	}

	llvm::BasicBlock* yieldbb;//location of starting point in generator function
	if (this->is_generator)
	{
		//compile the start code for a generator function
		auto data = function_context->Load("_context");

		//add arguments
		int i = 0;
		for (auto ii : *this->args)
		{
			auto ptr = data.val;
			auto val = function_context->root->builder.CreateGEP(ptr, { function_context->root->builder.getInt32(0), function_context->root->builder.getInt32(2 + i++) });
			function_context->RegisterLocal(ii.name.text, CValue(function_context->root->LookupType(ii.type.text)->GetPointerType(), val));
		}

		//add local vars
		for (unsigned int i = 2 + this->args->size(); i < data.type->base->data->struct_members.size(); i++)
		{
			auto gep = context->root->builder.CreateGEP(data.val, { context->root->builder.getInt32(0), context->root->builder.getInt32(i) });
			function_context->function->generator.variable_geps.push_back(gep);
		}

		//branch to the continue point
		auto br = function_context->root->builder.CreateGEP(data.val, { context->root->builder.getInt32(0), context->root->builder.getInt32(0) });
		auto loc = function_context->root->builder.CreateLoad(br);
		auto ibr = function_context->root->builder.CreateIndirectBr(loc, 10);//todo this number may need to be different
		function_context->function->generator.ibr = ibr;

		//add new bb
		yieldbb = llvm::BasicBlock::Create(context->context, "yield", function_context->function->f);
		ibr->addDestination(yieldbb);

		function_context->root->builder.SetInsertPoint(yieldbb);
	}

	block->Compile(function_context);

	//check for return, and insert one or error if there isnt one
	if (function_context->function->is_generator)
		function_context->root->builder.CreateRet(function_context->root->builder.getInt1(false));//signal we are gone generating values
	else if (function_context->function->f->getBasicBlockList().back().getTerminator() == 0)
		if (ret->type == Jet::Types::Void)// Implicit return void at end to satisfy llvm
			function_context->Return(CValue());
		else
			context->root->Error("Function must return a value!", token);

	//remove instructions after the first terminator in a block to prevent issues
	for (auto ii = function_context->function->f->getBasicBlockList().begin(); ii != function_context->function->f->getBasicBlockList().end(); ii++)
	{
		bool returned = false;
		for (auto inst = ii->begin(); inst != ii->end();)
		{
			if (returned == true)
			{
				auto temp = inst.getNodePtr();
				inst++;
				temp->getIterator()->eraseFromParent();//this could be broken
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

	if (this->is_generator)
	{
		//compile the other function necessary for an iterator
		auto func = context->AddFunction(this->GetRealName(), 0, { argsv.front() }, struct_name.length() > 0 ? argsv[0].first : 0, is_lambda);

		auto str = func->function->return_type;

		context->root->current_function = func;
		func->function->Load(context->root);

		//alloca the new context
		auto alloc = context->root->builder.CreateAlloca(str->GetLLVMType());

		//set the branch location to the start
		auto ptr = context->root->builder.CreateGEP(alloc, { context->root->builder.getInt32(0), context->root->builder.getInt32(0) });
		auto val = llvm::BlockAddress::get(yieldbb);
		context->root->builder.CreateStore(val, ptr);

		//ok, need to use the ret val stuff
		//store arguments into context
		if (argsv.size() > 1)
		{
			auto AI = func->function->f->arg_begin();
			AI++;//skip the return value argument
			for (unsigned int i = 1; i < argsv.size(); i++)
			{
				auto ptr = context->root->builder.CreateGEP(alloc, { context->root->builder.getInt32(0), context->root->builder.getInt32(2 + i - 1) });
				context->root->builder.CreateStore(AI++, ptr);
			}
		}

		//then return the newly created iterator object by storing it into the first arg
		auto sptr = context->root->builder.CreatePointerCast(alloc, context->root->CharPointerType->GetLLVMType());
		auto dptr = context->root->builder.CreatePointerCast(func->function->f->arg_begin(), context->root->CharPointerType->GetLLVMType());
		context->root->builder.CreateMemCpy(dptr, sptr, str->GetSize(), 1);

		context->root->builder.CreateRetVoid();

		//now compile reset function
		{
			auto reset = argsv.front().first->base->data->functions.find("Reset")->second->context;
			context->root->current_function = reset;
			reset->function->Load(reset->root);
			llvm::BasicBlock *bb = llvm::BasicBlock::Create(context->root->context, "entry", reset->function->f);
			context->root->builder.SetInsertPoint(bb);

			auto self = reset->function->f->arg_begin();
			//set the branch location back to start
			auto ptr = context->root->builder.CreateGEP(self, { context->root->builder.getInt32(0), context->root->builder.getInt32(0) });
			auto val = llvm::BlockAddress::get(yieldbb);
			context->root->builder.CreateStore(val, ptr);

			context->root->builder.CreateRetVoid();
		}
		//compile current function
		{
			auto current = argsv.front().first->base->data->functions.find("Current")->second->context;
			context->root->current_function = current;
			current->function->Load(current->root);
			llvm::BasicBlock *bb = llvm::BasicBlock::Create(context->root->context, "entry", current->function->f);
			context->root->builder.SetInsertPoint(bb);

			//return the current value
			auto self = current->function->f->arg_begin();
			auto ptr = context->root->builder.CreateGEP(self, { context->root->builder.getInt32(0), context->root->builder.getInt32(1) });
			context->root->builder.CreateRet(context->root->builder.CreateLoad(ptr));
		}
	}

	//reset insertion point to where it was before (for lambdas and template compilation)
	context->root->builder.SetCurrentDebugLocation(dp);
	if (rp)
		context->root->builder.SetInsertPoint(rp);
	context->root->current_function = context;

	//store the lambda value
	if (lambda)
	{
		function_context->WriteCaptures(lambda);

		auto ptr = context->root->builder.CreateGEP(lambda, { context->root->builder.getInt32(0), context->root->builder.getInt32(0) }, "name");

		ptr = context->root->builder.CreatePointerCast(ptr, function_context->function->f->getType()->getPointerTo());
		context->root->builder.CreateStore(function_context->function->f, ptr);
	}

	context->CurrentToken(&this->token);
	if (lambda)//return the lambda if we are one
		return CValue(lambda_type, context->root->builder.CreateLoad(lambda), lambda);
	else
		return CValue(function_context->function->GetType(context->root), function_context->function->f);
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
	fun->is_virtual = (this->token.type == TokenType::Virtual);
	context->root->functions.push_back(fun);
	auto str = dynamic_cast<StructExpression*>(this->parent) ? dynamic_cast<StructExpression*>(this->parent)->GetName() : this->Struct.text;

	if (auto attr = dynamic_cast<AttributeExpression*>(this->parent))
	{
		//add the attribute to the Function
		if (attr->name.text == "stdcall")
			fun->calling_convention = CallingConvention::StdCall;
		else if (attr->name.text == "thiscall")
			fun->calling_convention = CallingConvention::ThisCall;
		else if (attr->name.text == "fastcall")
			fun->calling_convention = CallingConvention::FastCall;
	}

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
	{
		context->root->ns->members.insert({ fname, fun });
	}


	if (is_generator)
	{
		//build data about the generator context struct
		Type* str = new Type;
		str->name = this->GetRealName() + "_yielder_context";
		str->type = Types::Struct;
		str->data = new Jet::Struct;
		str->data->name = str->name;
		str->data->parent_struct = 0;
		str->data->parent = context->root->ns;
		context->root->ns->members.insert({ str->name, str });


		//add default iterator methods, will fill in function later
		{
			auto func = new Function(this->GetRealName() + "_generator", name.text.length() == 0);
			func->return_type = context->root->BoolType;
			func->arguments = { { 0, "_context" } };
			func->arguments.resize(1);
			context->root->AdvanceTypeLookup(&func->arguments[0].first, str->name + "*", &this->ret_type);

			auto n = new CompilerContext(context->root, context);
			n->function = func;
			func->context = n;
			context->root->ns->members.insert({ func->name, func });

			str->data->functions.insert({ "MoveNext", func });
		}
		{
			auto func = new Function(this->GetRealName() + "_yield_reset", name.text.length() == 0);
			func->return_type = &VoidType;
			func->arguments = { { 0, "_context" } };
			func->arguments.resize(1);
			context->root->AdvanceTypeLookup(&func->arguments[0].first, str->name + "*", &this->ret_type);

			auto n = new CompilerContext(context->root, context);
			n->function = func;
			func->context = n;
			context->root->ns->members.insert({ func->name, func });

			str->data->functions.insert({ "Reset", func });
		}
		{
			auto func = new Function(this->GetRealName() + "_generator_current", name.text.length() == 0);
			func->return_type = &VoidType;
			context->root->AdvanceTypeLookup(&func->return_type, this->ret_type.text, &this->ret_type);

			func->arguments = { { 0, "_context" } };
			func->arguments.resize(1);
			context->root->AdvanceTypeLookup(&func->arguments[0].first, str->name + "*", &this->ret_type);

			auto n = new CompilerContext(context->root, context);
			n->function = func;
			func->context = n;
			context->root->ns->members.insert({ func->name, func });
			str->data->functions.insert({ "Current", func });
		}

		//add the position and return variables to the context
		str->data->struct_members.push_back({ "position", "char*", context->root->CharPointerType });
		str->data->struct_members.push_back({ "return", this->ret_type.text, context->root->LookupType(this->ret_type.text) });

		//add arguments to context
		for (auto ii : *this->args)
			str->data->struct_members.push_back({ ii.name.text, ii.type.text, context->root->LookupType(ii.type.text) });

		str->ns = context->root->ns;

		//DO NOT LOAD THIS UNTIL COMPILATION, IN FACT DONT LOAD ANYTHING
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
		if (this->templates)//just set type = 0 if it is one of the templates
		{
			bool done = false;
			for (auto temp : *this->templates)
			{
				//get the name of the variable
				unsigned int subl = 0;
				for (; subl < ii.type.text.length(); subl++)
				{
					if (!IsLetter(ii.type.text[subl]))
						break;
				}
				//check if it refers to same type
				std::string sub = ii.type.text.substr(0, subl);
				if (temp.second.text == sub)
				{
					//insert dummy types
					fun->arguments.push_back({ new Type(ii.type.text, Types::Invalid), ii.name.text });
					done = true;
					break;
				}
			}
			if (done)
				continue;
		}

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
		//add the attribute to the Function
		if (attr->name.text == "stdcall")
			fun->calling_convention = CallingConvention::StdCall;
		else if (attr->name.text == "thiscall")
			fun->calling_convention = CallingConvention::ThisCall;
		else if (attr->name.text == "fastcall")
			fun->calling_convention = CallingConvention::FastCall;
	}
	context->root->AdvanceTypeLookup(&fun->return_type, this->ret_type.text, &this->ret_type);

	// Reserve space for the arguments + this if we apply to a struct
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

CValue LetExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&(*_names)[0].second);

	// If im in global scope, add a global variable
	if (this->parent->parent == 0)
	{
		auto type = context->root->LookupType(this->_names->front().first.text);

		//should I add a constructor?
		llvm::Constant* cval = 0;
		if (this->_right && this->_right->size() > 0)
		{
			auto val = this->_right->front().second->Compile(context);
			cval = llvm::dyn_cast<llvm::Constant>(val.val);
			if (cval == 0)
				context->root->Error("Cannot instantiate global with non constant value", token);
		}

		if (this->_names->front().first.text.back() == ']')
		{
			std::string len = this->_names->front().first.text;
			len = len.substr(len.find_first_of('[') + 1);
			context->root->AddGlobal(this->_names->front().second.text, type->base, std::atoi(len.c_str()));
		}
		else
		{
			context->root->AddGlobal(this->_names->front().second.text, type, 0, cval);
		}

		return CValue();
	}

	bool needs_destruction = false;
	int i = 0;
	for (auto ii : *this->_names) {
		auto aname = ii.second.text;

		Type* type = 0;
		CValue val;
		llvm::AllocaInst* Alloca = 0;
		if (ii.first.text.length())
		{
			context->CurrentToken(&ii.first);
			type = context->root->LookupType(ii.first.text);
			context->CurrentToken(&(*_names)[0].second);
			if (this->_right)
				val = (*this->_right)[i++].second->Compile(context);
		}
		else if (this->_right)
		{
			val = (*this->_right)[i++].second->Compile(context);
			type = val.type;
		}

		if (context->function->is_generator)// Add arguments to variable symbol table.
		{
			//find the already added type with the same name
			auto ty = context->function->arguments[0].first->base;
			auto var_ptr = context->function->generator.variable_geps[context->function->generator.var_num++];

			if (this->_right)
			{
				val = context->DoCast(type, val);
				context->root->builder.CreateStore(val.val, var_ptr);
			}

			//output debug info
			llvm::DIFile* unit = context->root->debug_info.file;
			type->Load(context->root);
			llvm::DILocalVariable* D = context->root->debug->createAutoVariable(context->function->scope, aname, unit, ii.second.line,
				type->GetDebugType(context->root));

			llvm::Instruction *Call = context->root->debug->insertDeclare(
				var_ptr, D, context->root->debug->createExpression(), llvm::DebugLoc::get(this->token.line, this->token.column, context->function->scope), context->root->builder.GetInsertBlock());
			Call->setDebugLoc(llvm::DebugLoc::get(ii.second.line, ii.second.column, context->function->scope));

			//still need to do store
			context->RegisterLocal(aname, CValue(type->GetPointerType(), var_ptr));
			continue;
		}
		else if (ii.first.text.length() > 0)//type was specified
		{
			context->CurrentToken(&ii.first);

			type = context->root->LookupType(ii.first.text);

			auto TheFunction = context->function->f;
			llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
				TheFunction->getEntryBlock().begin());

			if (type->type == Types::Struct && type->data->templates.size() > 0)
				context->root->Error("Missing template arguments for type '" + type->ToString() + "'", ii.first);
			else if (type->type == Types::InternalArray)
			{
				if (this->_right)
				{
					context->root->Error("Cannot assign to a sized array type!", ii.second);
				}

				needs_destruction = true;
				Alloca = TmpB.CreateAlloca(type->GetLLVMType(), TmpB.getInt32(1), aname);
			}
			else if (type->type == Types::Array)
			{
				//if (type->size)
				//	needs_destruction = true;

				int length = 0;// type->size;

				auto str_type = context->root->GetArrayType(type->base);
				//type = str_type;
				//alloc the struct for it
				Alloca = TmpB.CreateAlloca(str_type->GetLLVMType(), TmpB.getInt32(1), aname);

				//allocate the array
				auto size = TmpB.getInt32(length);
				auto arr = TmpB.CreateAlloca(type->base->GetLLVMType(), size, aname + ".array");
				//store size
				auto size_p = TmpB.CreateGEP(Alloca, { TmpB.getInt32(0), TmpB.getInt32(0) });
				TmpB.CreateStore(size, size_p);

				//store pointer (todo write zero?)
				//auto pointer_p = TmpB.CreateGEP(Alloca, { TmpB.getInt32(0), TmpB.getInt32(1) });
				//TmpB.CreateStore(arr, pointer_p);
			}
			else if (type->GetBaseType()->type == Types::Trait)
			{
				context->root->Error("Cannot instantiate trait", ii.second);
			}
			else
			{
				Alloca = TmpB.CreateAlloca(type->GetLLVMType(), 0, aname);
			}

			if (type->GetSize() >= 4)
				Alloca->setAlignment(4);

			// Store the initial value into the alloca.
			if (this->_right)
			{
				CValue alloc;
				alloc.type = type->GetPointerType();
				alloc.val = Alloca;
				context->Store(alloc, val, true);
			}
		}
		else if (this->_right)
		{
			//need to move allocas outside of the loop and into the main body
			auto TheFunction = context->function->f;
			llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
				TheFunction->getEntryBlock().begin());

			Alloca = TmpB.CreateAlloca(val.type->GetLLVMType(), 0, aname);

			if (val.type->GetSize() >= 4)
				Alloca->setAlignment(4);

			CValue alloc;
			alloc.val = Alloca;
			alloc.type = val.type->GetPointerType();
			context->Store(alloc, val, true);
		}
		else
		{
			context->root->Error("Cannot infer variable type without a value!", ii.second);
		}

		// Add debug info
		llvm::DIFile* unit = context->root->debug_info.file;
		type->Load(context->root);
		llvm::DILocalVariable* D = context->root->debug->createAutoVariable(context->function->scope, aname, unit, ii.second.line,
			type->GetDebugType(context->root));

		llvm::Instruction *declare = context->root->debug->insertDeclare(
			Alloca, D, context->root->debug->createExpression(), llvm::DebugLoc::get(this->token.line, this->token.column, context->function->scope), context->root->builder.GetInsertBlock());
		declare->setDebugLoc(llvm::DebugLoc::get(ii.second.line, ii.second.column, context->function->scope));

		context->RegisterLocal(aname, CValue(type->GetPointerType(), Alloca), needs_destruction);

		//construct it!
		if (this->_right == 0)
		{
			if (type->type == Types::Struct)
			{
				context->Construct(CValue(type->GetPointerType(), Alloca), 0);
			}
			else if (type->type == Types::Array && type->base->type == Types::Struct)
			{
				//todo lets move this junk into construct so we dont have to do this in multiple places
				auto loc = context->root->builder.CreateGEP(Alloca, { context->root->builder.getInt32(0), context->root->builder.getInt32(0) });
				auto size = context->root->builder.CreateLoad(loc);

				auto ptr = context->root->builder.CreateGEP(Alloca, { context->root->builder.getInt32(0), context->root->builder.getInt32(1) });
				ptr = context->root->builder.CreateLoad(ptr);
				context->Construct(CValue(type->base->GetPointerType(), ptr), size);
			}
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

	auto Struct = this->GetName();
	//need to initialize virtual tables
	Type* str = context->root->LookupType(Struct);
	std::string strname = str->data->template_base ? str->data->template_base->name : str->data->name;

	for (auto ii : str->data->functions)
	{
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

				CompilerContext* function = context->AddFunction(ii.second->name, ret, argsv, Struct.length() > 0 ? str : 0, false);// , this->varargs);
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

					auto local = context->root->debug->createAutoVariable(
						function->function->scope, aname, context->root->debug_info.file, this->token.line,
						argsv[Idx].first->GetDebugType(context->root));

					llvm::Instruction *call = context->root->debug->insertDeclare(
						Alloca, local, context->root->debug->createExpression(), 
						llvm::DebugLoc::get(this->token.line, this->token.column, function->function->scope), Alloca);
					call->setDebugLoc(llvm::DebugLoc::get(this->token.line, this->token.column, function->function->scope));

					// Add arguments to variable symbol table.
					function->RegisterLocal(aname, CValue(argsv[Idx].first->GetPointerType(), Alloca));
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
				argsv.push_back({ str->GetPointerType(), "this" });

				auto ret = &VoidType;

				CompilerContext* function = context->AddFunction(ii.second->name, ret, argsv, Struct.length() > 0 ? str : 0, false);// , this->varargs);
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

					auto D = context->root->debug->createAutoVariable(function->function->scope, aname, unit, this->token.line,
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
				context->root->builder.SetInsertPoint(rp);
		}
	}
}


void StructExpression::CompileDeclarations(CompilerContext* context)
{
	//build data about the struct
	Type* str = new Type(this->name.text, Types::Struct, new Struct);
	if (this->token.type == TokenType::Class)
		str->data->is_class = true;

	context->root->ns->members.insert({ this->name.text, str });
	str->ns = context->root->ns;

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
				context->root->AdvanceTypeLookup(&str->data->struct_members.back().type, ii.variable.type.text, &ii.variable.type);
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
				auto func = new Function(ii.function->GetRealName(), false);
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
