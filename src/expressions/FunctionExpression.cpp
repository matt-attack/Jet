#include "FunctionExpression.h"
#include "ControlExpressions.h"
#include "DeclarationExpressions.h"
#include "StructExpression.h"
#include "Expressions.h"

#include <Lexer.h>

#include <types/Function.h>

#include <CompilerContext.h>

using namespace Jet;

FunctionExpression::~FunctionExpression()
{
	delete block;
	delete varargs;
	delete args;
	delete captures;
}

void FunctionExpression::SetParent(Expression* parent)
{
	this->parent = parent;
	block->SetParent(this);
}

void FunctionExpression::Visit(ExpressionVisitor* visitor)
{
	visitor->Visit(this);
	block->Visit(visitor);
}

void FunctionExpression::Print(std::string& output, Source* source)
{
	if (this->name.text.length() == 0)
		throw 7;//todo: implement lamdbas
	//add tokens for the ( )
	token.Print(output, source);

	ret_type.Print(output, source);

	if (this->Struct.text.length())
	{
		this->Struct.Print(output, source);
		//add the ::
		this->colons.Print(output, source);
	}

	if (this->oper.text.length())
		this->oper.Print(output, source);

	name.Print(output, source);
	
	open_bracket.Print(output, source);

	for (auto ii : *this->args)
	{
		ii.type.Print(output, source);
		ii.name.Print(output, source);
		if (ii.comma.text.length())
			ii.comma.Print(output, source);
	}

	close_bracket.Print(output, source);

	this->block->Print(output, source);
}

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
		nc->function->return_type_ = context->root->LookupType(this->ret_type.text, false);

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
		nc->function->return_type_ = context->root->LookupType(this->ret_type.text, false);

		for (auto ii : *this->args)
			nc->TCRegisterLocal(ii.name.text, context->root->LookupType(ii.type.text)->GetPointerType());
	}

	if (this->is_generator)
	{
		auto str = myself->return_type_;
		//add _context
		nc->TCRegisterLocal("_context", str->GetPointerType()->GetPointerType());

		nc->local_reg_callback = [&](const std::string& name, Type* ty)
		{
			str->data->struct_members.push_back({ name, ty->base->name, ty->base });
		};

		this->block->TypeCheck(nc);

		if (this->ret_type.text == "void")
			;
		else if (nc->function->has_return_ == false && this->is_generator == false)
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

CValue FunctionExpression::Compile(CompilerContext* context)
{
	auto Struct = dynamic_cast<StructExpression*>(this->parent) ? dynamic_cast<StructExpression*>(this->parent)->GetName() : this->Struct.text;

	context->CurrentToken(&this->token);
	//need to not compile if template or trait
	if (this->templates)
	{
		for (auto ii : *this->templates)//make sure traits are valid
		{
			auto iter = context->root->traits.find(ii.first.text);
			if (iter == context->root->traits.end() || iter->second->valid == false)
				context->root->Error("Trait '" + ii.first.text + "' is not defined", ii.first);
		}
		return CValue(context->root->VoidType, 0);
	}

	if (Struct.length())
	{
		auto iter = context->root->LookupType(Struct, false);
		if (iter->type == Types::Trait)
			return CValue(context->root->VoidType, 0);
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

	//build list of types of vars
	std::vector<std::pair<Type*, std::string>> argsv;
	auto struct_name = dynamic_cast<StructExpression*>(this->parent) ? dynamic_cast<StructExpression*>(this->parent)->GetName() : this->Struct.text;

	//insert the 'this' argument if I am a member function
	if (struct_name.length() > 0)
	{
		argsv.push_back({ context->root->LookupType(struct_name), "this" });
	}

	//add the context pointer as an argument if this is a generator
	if (this->is_generator)
	{
		auto str = myself->return_type_;
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
				context->root->Error("Cannot imply type of lambda with the args of its call", context->current_token);

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
					context->root->Error("Lambda type inference only implemented for function calls", context->current_token);
			if (this->ret_type.text.length() == 0)
				context->root->Error("Lambda type inference only implemented for function calls", context->current_token);
		}
	}

	for (auto& ii : *this->args)
	{
		context->CurrentToken(&ii.type);
		argsv.push_back({ context->root->LookupType(ii.type.text), ii.name.text });
	}

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
		for (auto& ii : argsv)
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
		function_context = argsv.front().first->base->data->functions.find("MoveNext")->second->context_;
		function_context->function->Load(context->root);
		llvm::BasicBlock *bb = llvm::BasicBlock::Create(context->root->context, "entry", function_context->function->f_);
		context->root->builder.SetInsertPoint(bb);

		function_context->function->is_generator_ = true;
	}
	else
	{
		// create the function if this was a lambda
		if (this->name.text.length() == 0)
		{
			myself = new Function(this->GetFunctionNamePrefix(), false);
			myself->expression_ = this;
			myself->is_virtual_ = false;
			myself->arguments_ = argsv;
			myself->return_type_ = ret;
            myself->is_lambda_ = true;
			context->root->functions.push_back(myself);
		}

		function_context = context->StartFunctionDefinition(myself);
	}

	function_context->function->lambda_.storage_type = storage_t;
	context->root->current_function = function_context;
	//function_context->function->Load(context->root);

	function_context->SetDebugLocation(this->token);

	// Setup arguments for the function
	auto AI = function_context->function->f_->arg_begin();
	if (function_context->function->return_type_->type == Types::Struct)
	{
		AI++;// skip the struct return argument, its not a real argument
	}
	for (unsigned i = 0, e = argsv.size(); i != e; ++i, ++AI)
	{
		// Skip actually adding all args but the first one if this is actually a generator
		if (i > 0 && this->is_generator)
			continue;

		auto& arg_name = argsv[i].second;
        auto& arg_type = argsv[i].first;

        // for now pass all structs by pointer/reference
        // eventually can do this based on size
        llvm::Value* storage = AI;
		AI->setName(arg_name);

		//insert debug declarations
		auto local = context->root->debug->createAutoVariable(function_context->function->scope_, arg_name,
			context->root->debug_info.file, this->token.line,
			arg_type->GetDebugType(context->root));

		llvm::Instruction* call = context->root->debug->insertDeclare(storage, local, context->root->debug->createExpression(),
			llvm::DebugLoc::get(this->token.line, this->token.column, function_context->function->scope_),
			context->root->builder.GetInsertBlock());
		call->setDebugLoc(llvm::DebugLoc::get(this->token.line, this->token.column, function_context->function->scope_));


        // Get the offset to apply for looking for the name token for this
        int offset = 0;
        offset += struct_name.length() > 0 ? 1 : 0;
        offset += this->is_generator ? 1 : 0;

        // Special case for this, always pass by pointer/reference.
        if (struct_name.length() && i == 0)
        {
          function_context->RegisterLocal(arg_name, CValue(arg_type, 0, storage), false, !!const_tok);
          continue;
        }

		// Add arguments to variable symbol table. These are always const.
        function_context->CurrentToken(&(*this->args)[i - offset].name);
        if (arg_type->type == Types::Struct)
        {
            // all structs are passed by pointer
		    function_context->RegisterLocal(arg_name, CValue(arg_type, 0, storage), false, true);
        }
        else
        {
            // everything else passed by value
            function_context->RegisterLocal(arg_name, CValue(arg_type, storage), false, true);
        }
	}

	llvm::BasicBlock* yieldbb;//location of starting point in generator function
	if (this->is_generator)
	{
		//compile the start code for a generator function
		auto data = function_context->Load("_context");

		//add arguments
		int i = 0;
		for (auto& ii : *this->args)
		{
			auto ptr = data.val;
			auto val = function_context->root->builder.CreateGEP(ptr, { function_context->root->builder.getInt32(0), function_context->root->builder.getInt32(2 + i++) });
			function_context->RegisterLocal(ii.name.text, CValue(function_context->root->LookupType(ii.type.text), 0, val));
		}

		//add local vars
		for (unsigned int i = 2 + this->args->size(); i < data.type->base->data->struct_members.size(); i++)
		{
			auto gep = context->root->builder.CreateGEP(data.val, { context->root->builder.getInt32(0), context->root->builder.getInt32(i) });
			function_context->function->generator_.variable_geps.push_back(gep);
		}

		//branch to the continue point
		auto br = function_context->root->builder.CreateGEP(data.val, { context->root->builder.getInt32(0), context->root->builder.getInt32(0) });
		auto loc = function_context->root->builder.CreateLoad(br);
		auto ibr = function_context->root->builder.CreateIndirectBr(loc, 10);//todo this number may need to be different
		function_context->function->generator_.ibr = ibr;

		//add new bb
		yieldbb = llvm::BasicBlock::Create(context->context, "yield", function_context->function->f_);
		ibr->addDestination(yieldbb);

		function_context->root->builder.SetInsertPoint(yieldbb);
	}

	block->Compile(function_context);

	//check for return, and insert one or error if there isnt one
	if (function_context->function->is_generator_)
		function_context->root->builder.CreateRet(function_context->root->builder.getInt1(false));//signal we are gone generating values
	else if (function_context->function->f_->getBasicBlockList().back().getTerminator() == 0)
		if (ret->type == Jet::Types::Void)// Implicit return void at end to satisfy llvm
			function_context->Return(CValue(context->root->VoidType, 0));
		else
			context->root->Error("Function must return a value!", token);

	//remove instructions after the first terminator in a block to prevent issues
	for (auto ii = function_context->function->f_->getBasicBlockList().begin(); ii != function_context->function->f_->getBasicBlockList().end(); ii++)
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
		context->root->builder.SetCurrentDebugLocation(0);

		//compile the other function necessary for an iterator
		auto func = context->StartFunctionDefinition(myself);

		auto str = func->function->return_type_;

		context->root->current_function = func;

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
			auto AI = func->function->f_->arg_begin();
			AI++;//skip the return value argument
			for (unsigned int i = 1; i < argsv.size(); i++)
			{
				auto ptr = context->root->builder.CreateGEP(alloc, { context->root->builder.getInt32(0), context->root->builder.getInt32(2 + i - 1) });
				context->root->builder.CreateStore(AI++, ptr);
			}
		}

		//then return the newly created iterator object by storing it into the first arg
		auto sptr = context->root->builder.CreatePointerCast(alloc, context->root->CharPointerType->GetLLVMType());
		auto dptr = context->root->builder.CreatePointerCast(func->function->f_->arg_begin(), context->root->CharPointerType->GetLLVMType());
		context->root->builder.CreateMemCpy(dptr, 0, sptr, 0, str->GetSize());// todo properly handle alignment

		context->root->builder.CreateRetVoid();

		//now compile reset function
		{
			auto reset = argsv.front().first->base->data->functions.find("Reset")->second->context_;
			context->root->current_function = reset;
			reset->function->Load(reset->root);
			llvm::BasicBlock *bb = llvm::BasicBlock::Create(context->root->context, "entry", reset->function->f_);
			context->root->builder.SetInsertPoint(bb);

			auto self = reset->function->f_->arg_begin();
			//set the branch location back to start
			auto ptr = context->root->builder.CreateGEP(self, { context->root->builder.getInt32(0), context->root->builder.getInt32(0) });
			auto val = llvm::BlockAddress::get(yieldbb);
			context->root->builder.CreateStore(val, ptr);

			context->root->builder.CreateRetVoid();
		}
		//compile current function
		{
			auto current = argsv.front().first->base->data->functions.find("Current")->second->context_;
			context->root->current_function = current;
			current->function->Load(current->root);
			llvm::BasicBlock *bb = llvm::BasicBlock::Create(context->root->context, "entry", current->function->f_);
			context->root->builder.SetInsertPoint(bb);

			//return the current value
			auto self = current->function->f_->arg_begin();
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
    auto llvm_f = function_context->function->f_;
	if (lambda)
	{
		function_context->WriteCaptures(lambda);

		auto ptr = context->root->builder.CreateGEP(lambda, { context->root->builder.getInt32(0), context->root->builder.getInt32(0) }, "name");

		ptr = context->root->builder.CreatePointerCast(ptr, llvm_f->getType()->getPointerTo());
		context->root->builder.CreateStore(llvm_f, ptr);
	}

	context->CurrentToken(&this->token);
	if (lambda)//return the lambda if we are one
		return CValue(lambda_type, context->root->builder.CreateLoad(lambda), lambda);
	else
		return CValue(function_context->function->GetType(context->root), llvm_f);
}

std::string FunctionExpression::GetFunctionNamePrefix()
{
	std::string fname;
	if (name.text.length() > 0)
		fname = name.text;
	else
		fname = "_lambda_";

	const auto& ns = this->GetNamespaceQualifier();
	if (ns.length() > 0)
		return ns + "_" + fname;
	return fname;
}

void FunctionExpression::CompileDeclarations(CompilerContext* context)
{
	context->CurrentToken(&this->token);

	if (name.text.length() == 0)
	{
		return;//dont compile declaration for lambdas
	}

	std::string name_prefix = this->GetFunctionNamePrefix();

	bool advlookup = true;
	auto str = dynamic_cast<StructExpression*>(this->parent) ? dynamic_cast<StructExpression*>(this->parent)->GetName() : this->Struct.text;


    if (!str.length() && !!const_tok)
    {
        // Error if someone specified const
        context->root->Error("Can only specify const for member functions", this->const_tok);
    }

	Function* fun = new Function(name_prefix, false);
	fun->expression_ = this;
	fun->is_virtual_ = (this->token.type == TokenType::Virtual);
    fun->is_const_ = !!const_tok;
	context->root->functions.push_back(fun);
	myself = fun;

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

	bool is_trait = false;
	const auto& fname = name.text;
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
        // make sure we dont have any duplicates
        auto dup = context->root->ns->members.equal_range(fname);
        for (auto it = dup.first; it != dup.second; it++)
        {
            if (it->second.type == SymbolType::Function &&
                it->second.fn->arguments_.size() != args->size())
            {
                continue;
            }
            // todo give details about the location of the existing symbol
            context->root->Error("A matching symbol with name '" + name.text + "' is already defined", this->name);
        }
		context->root->ns->members.insert({ fname, fun });
	}


	if (is_generator)
	{
		//build data about the generator context struct
		Type* str = new Type(context->root, name_prefix + "_yielder_context", Types::Struct);
		str->data = new Jet::Struct;
		str->data->name = str->name;
		str->data->parent_struct = 0;
		str->data->parent = context->root->ns;
		context->root->ns->members.insert({ str->name, str });


		//add default iterator methods, will fill in function later
		{
			auto func = new Function(name_prefix + "_generator", name.text.length() == 0);
			func->return_type_ = context->root->BoolType;
			func->arguments_ = { { 0, "_context" } };
			func->arguments_.resize(1);
			context->root->AdvanceTypeLookup(&func->arguments_[0].first, str->name + "*", &this->ret_type);
			context->root->functions.push_back(func);

			auto n = new CompilerContext(context->root, context);
			n->function = func;
			func->context_ = n;
			context->root->ns->members.insert({ func->name_, func });

			str->data->functions.insert({ "MoveNext", func });
		}
		{
			auto func = new Function(name_prefix + "_yield_reset", name.text.length() == 0);
			func->return_type_ = context->root->VoidType;
			func->arguments_ = { { 0, "_context" } };
			func->arguments_.resize(1);
			context->root->AdvanceTypeLookup(&func->arguments_[0].first, str->name + "*", &this->ret_type);
			context->root->functions.push_back(func);

			auto n = new CompilerContext(context->root, context);
			n->function = func;
			func->context_ = n;
			context->root->ns->members.insert({ func->name_, func });

			str->data->functions.insert({ "Reset", func });
		}
		{
			auto func = new Function(name_prefix + "_generator_current", name.text.length() == 0);
			func->return_type_ = context->root->VoidType;
			context->root->AdvanceTypeLookup(&func->return_type_, this->ret_type.text, &this->ret_type);
			context->root->functions.push_back(func);

			func->arguments_ = { { 0, "_context" } };
			func->arguments_.resize(1);
			context->root->AdvanceTypeLookup(&func->arguments_[0].first, str->name + "*", &this->ret_type);

			auto n = new CompilerContext(context->root, context);
			n->function = func;
			func->context_ = n;
			context->root->ns->members.insert({ func->name_, func });
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
		fun->return_type_ = str;
	}
	else if (is_trait)
		fun->return_type_ = 0;
	else if (advlookup)
		context->root->AdvanceTypeLookup(&fun->return_type_, this->ret_type.text, &this->ret_type);
	else
		fun->return_type_ = context->root->LookupType(this->ret_type.text, false);

	fun->arguments_.reserve(this->args->size() + (str.length() ? 1 : 0));

	//add the this pointer argument if this is a member function
	if (str.length() > 0)
	{
		fun->arguments_.push_back({ 0, "this" });
		if (is_trait == false && advlookup)
			context->root->AdvanceTypeLookup(&fun->arguments_.back().first, str + "*", &this->token);
		else if (is_trait == false)
			fun->arguments_.back().first = context->root->LookupType(str + "*", false);
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
					fun->arguments_.push_back({ new Type(context->root, ii.type.text, Types::Invalid), ii.name.text });
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

		fun->arguments_.push_back({ type, ii.name.text });
		if (advlookup && is_trait == false)
			context->root->AdvanceTypeLookup(&fun->arguments_.back().first, ii.type.text, &this->token);
	}

	//add templates to new function
	if (this->templates)
	{
		for (auto ii : *this->templates)
        {
			fun->templates_.push_back({ context->root->LookupType(ii.first.text), ii.second.text });
        }
	}
}

