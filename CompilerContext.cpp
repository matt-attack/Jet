#include "Compiler.h"
#include "CompilerContext.h"
#include "Types/Function.h"

#include "Lexer.h"
#include "Expressions.h"
#include "DeclarationExpressions.h"

using namespace Jet;

CompilerContext* CompilerContext::AddFunction(const std::string& fname, Type* ret, const std::vector<std::pair<Type*, std::string>>& args, Type* member, bool lambda)
{
	auto iter = root->ns->GetFunction(fname);
	Function* func = 0;
	if (member)
	{
		//std::string str;
		//int i = 2;
		//for (; i < fname.length(); i++)
		//{
		//	if (fname[i] == '_')
		//		break;

		//	str += fname[i];
		//}
		//ok, if member just pass the type so we dont have to do this crap
		auto type = member;// this->root->LookupType(str);
		int i = member->name.length() + 2;
		std::string fname2 = fname.substr(++i, fname.length() - i);

		auto range = type->data->functions.equal_range(fname2);
		for (auto ii = range.first; ii != range.second; ii++)
		{
			//printf("found option for %s with %i args\n", fname.c_str(), ii->second->argst.size());
			if (ii->second->arguments.size() == args.size())
				func = ii->second;
		}
	}
	if (iter == 0 && member == false)
	{
		//no function exists
		func = new Function(fname, lambda);
		func->return_type = ret;
		func->arguments = args;

		auto n = new CompilerContext(this->root, this);
		n->function = func;
		func->context = n;
		func->Load(this->root);

		if (member == false)
			this->root->ns->members.insert({ fname, func });

		llvm::BasicBlock *bb = llvm::BasicBlock::Create(root->context, "entry", n->function->f);
		root->builder.SetInsertPoint(bb);
		func->loaded = true;

		return n;
	}
	else if (func == 0)
	{
		//select the right one
		func = root->ns->GetFunction(fname/*, args*/);

		if (func == 0)
			this->root->Error("Function '" + fname + "' not found", *this->root->current_function->current_token);
	}

	func->Load(this->root);

	auto n = new CompilerContext(this->root, this);
	n->function = func;
	func->context = n;
	llvm::BasicBlock *bb = llvm::BasicBlock::Create(root->context, "entry", n->function->f);
	root->builder.SetInsertPoint(bb);

	this->root->current_function = n;
	return n;
}

CValue CompilerContext::UnaryOperation(TokenType operation, CValue value)
{
	llvm::Value* res = 0;

	if (operation == TokenType::BAnd)
	{
		//this should already have been done elsewhere, so error
		assert(false);
	}

	if (value.type->type == Types::Float || value.type->type == Types::Double)
	{
		switch (operation)
		{
		case TokenType::Minus:
			res = root->builder.CreateFNeg(value.val);
			break;
		default:
			this->root->Error("Invalid Unary Operation '" + TokenToString[operation] + "' On Type '" + value.type->ToString() + "'", *current_token);
			break;
		}

		return CValue(value.type, res);
	}
	else if (value.type->type == Types::Int || value.type->type == Types::Short || value.type->type == Types::Char)
	{
		//integer probably
		switch (operation)
		{
		case TokenType::Increment:
			res = root->builder.CreateAdd(value.val, root->builder.getInt32(1));
			break;
		case TokenType::Decrement:
			res = root->builder.CreateSub(value.val, root->builder.getInt32(1));
			break;
		case TokenType::Minus:
			res = root->builder.CreateNeg(value.val);
			break;
		case TokenType::BNot:
			res = root->builder.CreateNot(value.val);
			break;
		default:
			this->root->Error("Invalid Unary Operation '" + TokenToString[operation] + "' On Type '" + value.type->ToString() + "'", *current_token);
			break;
		}

		return CValue(value.type, res);
	}
	else if (value.type->type == Types::Pointer)
	{
		switch (operation)
		{
		case TokenType::Asterisk:
			return CValue(value.type->base, this->root->builder.CreateLoad(value.val));
		case TokenType::Increment:
			return CValue(value.type, this->root->builder.CreateGEP(value.val, root->builder.getInt32(1)));
		case TokenType::Decrement:
			return CValue(value.type, this->root->builder.CreateGEP(value.val, root->builder.getInt32(-1)));
		default:
			this->root->Error("Invalid Unary Operation '" + TokenToString[operation] + "' On Type '" + value.type->base->ToString() + "'", *current_token);
		}

	}
	else if (value.type->type == Types::Bool)
	{
		switch (operation)
		{
		case TokenType::Not:
			return CValue(value.type, this->root->builder.CreateNot(value.val));
		default:
			break;
		}
	}
	this->root->Error("Invalid Unary Operation '" + TokenToString[operation] + "' On Type '" + value.type->ToString() + "'", *current_token);
}

CValue CompilerContext::BinaryOperation(Jet::TokenType op, CValue left, CValue right)
{
	llvm::Value* res = 0;

	//should this be a floating point calc?
	if (left.type->type != right.type->type)
	{
		//conversion time!!
		root->Error("Cannot perform a binary operation between two incompatible types", *this->current_token);
	}

	if (left.type->type == Types::Float || left.type->type == Types::Double)
	{
		switch (op)
		{
		case TokenType::AddAssign:
		case TokenType::Plus:
			res = root->builder.CreateFAdd(left.val, right.val);
			break;
		case TokenType::SubtractAssign:
		case TokenType::Minus:
			res = root->builder.CreateFSub(left.val, right.val);
			break;
		case TokenType::MultiplyAssign:
		case TokenType::Asterisk:
			res = root->builder.CreateFMul(left.val, right.val);
			break;
		case TokenType::DivideAssign:
		case TokenType::Slash:
			res = root->builder.CreateFDiv(left.val, right.val);
			break;
		case TokenType::LessThan:
			//use U or O?
			res = root->builder.CreateFCmpULT(left.val, right.val);
			return CValue(root->BoolType, res);
			break;
		case TokenType::LessThanEqual:
			res = root->builder.CreateFCmpULE(left.val, right.val);
			return CValue(root->BoolType, res);
			break;
		case TokenType::GreaterThan:
			res = root->builder.CreateFCmpUGT(left.val, right.val);
			return CValue(root->BoolType, res);
			break;
		case TokenType::GreaterThanEqual:
			res = root->builder.CreateFCmpUGE(left.val, right.val);
			return CValue(root->BoolType, res);
			break;
		case TokenType::Equals:
			res = root->builder.CreateFCmpUEQ(left.val, right.val);
			return CValue(root->BoolType, res);
			break;
		case TokenType::NotEqual:
			res = root->builder.CreateFCmpUNE(left.val, right.val);
			return CValue(root->BoolType, res);
			break;
		default:
			this->root->Error("Invalid Binary Operation '" + TokenToString[op] + "' On Type '" + left.type->ToString() + "'", *current_token);

			break;
		}

		return CValue(left.type, res);
	}
	else if (left.type->type == Types::Int || left.type->type == Types::Short || left.type->type == Types::Char)
	{
		//integer probably
		switch (op)
		{
		case TokenType::AddAssign:
		case TokenType::Plus:
			res = root->builder.CreateAdd(left.val, right.val);
			break;
		case TokenType::SubtractAssign:
		case TokenType::Minus:
			res = root->builder.CreateSub(left.val, right.val);
			break;
		case TokenType::MultiplyAssign:
		case TokenType::Asterisk:
			res = root->builder.CreateMul(left.val, right.val);
			break;
		case TokenType::DivideAssign:
		case TokenType::Slash:
			if (true)//signed
				res = root->builder.CreateSDiv(left.val, right.val);
			else//unsigned
				res = root->builder.CreateUDiv(left.val, right.val);
			break;
		case TokenType::Modulo:
			if (true)//signed
				res = root->builder.CreateSRem(left.val, right.val);
			else//unsigned
				res = root->builder.CreateURem(left.val, right.val);
			break;
			//todo add unsigned
		case TokenType::LessThan:
			//use U or S?
			res = root->builder.CreateICmpSLT(left.val, right.val);
			return CValue(root->BoolType, res);
			break;
		case TokenType::LessThanEqual:
			res = root->builder.CreateICmpSLE(left.val, right.val);
			return CValue(root->BoolType, res);
			break;
		case TokenType::GreaterThan:
			res = root->builder.CreateICmpSGT(left.val, right.val);
			return CValue(root->BoolType, res);
			break;
		case TokenType::GreaterThanEqual:
			res = root->builder.CreateICmpSGE(left.val, right.val);
			return CValue(root->BoolType, res);
			break;
		case TokenType::Equals:
			res = root->builder.CreateICmpEQ(left.val, right.val);
			return CValue(root->BoolType, res);
			break;
		case TokenType::NotEqual:
			res = root->builder.CreateICmpNE(left.val, right.val);
			return CValue(root->BoolType, res);
			break;
		case TokenType::BAnd:
		case TokenType::AndAssign:
			res = root->builder.CreateAnd(left.val, right.val);
			break;
		case TokenType::BOr:
		case TokenType::OrAssign:
			res = root->builder.CreateOr(left.val, right.val);
			break;
		case TokenType::Xor:
		case TokenType::XorAssign:
			res = root->builder.CreateXor(left.val, right.val);
			break;
		case TokenType::LeftShift:
			//todo
			res = root->builder.CreateShl(left.val, right.val);
			break;
		case TokenType::RightShift:
			//todo
			res = root->builder.CreateLShr(left.val, right.val);
			break;
		default:
			this->root->Error("Invalid Binary Operation '" + TokenToString[op] + "' On Type '" + left.type->ToString() + "'", *current_token);

			break;
		}

		return CValue(left.type, res);
	}

	this->root->Error("Invalid Binary Operation '" + TokenToString[op] + "' On Type '" + left.type->ToString() + "'", *current_token);
}

Function* CompilerContext::GetMethod(const std::string& name, const std::vector<Type*>& args, Type* Struct)
{
	Function* fun = 0;

	if (Struct == 0)
	{
		//global function?
		auto iter = this->root->GetFunction(name);// functions.find(name);
		if (iter == 0)//this->root->functions.end())
		{
			//check if its a type, if so try and find a constructor
			auto type = this->root->TryLookupType(name);// types.find(name);// LookupType(name);
			if (type != 0 && type->type == Types::Struct)
			{
				type->Load(this->root);
				//look for a constructor
				//if we are template, remove the templated part
				auto tmp_name = name;
				if (name.back() == '>')
					tmp_name = name.substr(0, name.find_first_of('<'));
				auto range = type->data->functions.equal_range(tmp_name);
				for (auto ii = range.first; ii != range.second; ii++)
				{
					if (ii->second->arguments.size() == args.size() + 1)
						fun = ii->second;
				}
				if (fun)
				{
					//ok, we allocate, call then 
					//allocate thing
					//auto Alloca = this->root->builder.CreateAlloca(GetType(type->second), 0, "constructortemp");

					//fun->Load(this->parent);

					return fun;// CValue(type->second, this->root->builder.CreateLoad(Alloca));
				}
			}
		}

		if (name[name.length() - 1] == '>')//its a template
		{
			int i = name.find_first_of('<');
			auto base_name = name.substr(0, i);

			//auto range = this->root->functions.equal_range(name);
			auto type = this->root->LookupType(name);

			if (type)
			{
				return type->data->functions.find(type->data->template_base->name)->second;
			}
			//instantiate here
			this->root->Error("Not implemented", *this->current_token);

			//return range.first->second;
		}

		//look for the best one
		fun = this->root->GetFunction(name, args);

		if (fun && fun->templates.size() > 0)
		{
			auto templates = new Type*[fun->templates.size()];
			for (int i = 0; i < fun->templates.size(); i++)
				templates[i] = 0;

			//need to infer
			if (fun->arguments.size() > 0)
			{
				int i = 0;
				for (auto ii : fun->templates)
				{
					//look for stuff in args
					int i2 = 0;
					for (auto iii : fun->arguments)
					{
						if (iii.first->name == ii.second)
						{
							//found it
							if (templates[i] != 0 && templates[i] != args[i2])
								this->root->Error("Could not infer template type", *this->current_token);

							templates[i] = args[i2];
						}
						i2++;
					}
					i++;
				}
			}

			for (int i = 0; i < fun->templates.size(); i++)
			{
				if (templates[i] == 0)
					this->root->Error("Could not infer template type", *this->current_token);
			}

			auto oldname = fun->expression->name.text;
			fun->expression->name.text += '<';
			for (int i = 0; i < fun->templates.size(); i++)
			{
				fun->expression->name.text += templates[i]->ToString();
				if (i + 1 < fun->templates.size())
					fun->expression->name.text += ',';
			}
			fun->expression->name.text += '>';
			auto rname = fun->expression->name.text;

			//register the types
			int i = 0;
			for (auto ii : fun->templates)
			{
				//check if traits match
				if (templates[i]->MatchesTrait(this->root, ii.first->trait) == false)
					root->Error("Type '" + templates[i]->name + "' doesn't match Trait '" + ii.first->name + "'", *root->current_function->current_token);

				root->ns->members.insert({ ii.second, templates[i++] });
			}

			//store then restore insertion point
			auto rp = root->builder.GetInsertBlock();
			auto dp = root->builder.getCurrentDebugLocation();


			fun->expression->CompileDeclarations(this);
			fun->expression->DoCompile(this);

			root->builder.SetCurrentDebugLocation(dp);
			if (rp)
				root->builder.SetInsertPoint(rp);

			fun->expression->name.text = oldname;

			//time to recompile and stuff
			return root->ns->members.find(rname)->second.fn;
		}
		return fun;
	}
	else
	{
		return Struct->GetMethod(name, args, this);
	}
}

CValue CompilerContext::Call(const std::string& name, const std::vector<CValue>& args, Type* Struct)
{
	auto old_tok = this->current_token;
	std::vector<Type*> arsgs;
	for (auto ii : args)
		arsgs.push_back(ii.type);
	Function* fun = this->GetMethod(name, arsgs, Struct);
	this->current_token = old_tok;
	//ok, having issues with constructors, need way to tell if they are one
	if (fun == 0 && Struct == 0)
	{
		//global function?
		//check if its a type, if so try and find a constructor
		auto type = this->root->TryLookupType(name);
		if (type != 0 && type->type == Types::Struct)
		{
			//look for a constructor
			auto range = type->data->functions.equal_range(name);
			for (auto ii = range.first; ii != range.second; ii++)
			{
				if (ii->second->arguments.size() == args.size())// + 1)
					fun = ii->second;
			}
			if (fun)
			{
				//ok, we allocate, call then 
				//allocate thing
				type->Load(this->root);
				auto Alloca = this->root->builder.CreateAlloca(type->GetLLVMType(), 0, "constructortemp");

				std::vector<llvm::Value*> argsv;
				int i = 1;

				//add struct
				argsv.push_back(Alloca);
				for (auto ii : args)
					argsv.push_back(this->DoCast(fun->arguments[i++].first, ii).val);//try and cast to the correct type if we can

				fun->Load(this->root);

				this->root->builder.CreateCall(fun->f, argsv);

				return CValue(type, this->root->builder.CreateLoad(Alloca));
			}
		}
		else
		{
			//try to find it in variables
			auto var = this->GetVariable(name);

			if (var.type->type != Types::Function && (var.type->type != Types::Pointer || var.type->base->type != Types::Function))
			{
				if (var.type->type == Types::Pointer && var.type->base->type == Types::Struct && var.type->base->data->template_base && var.type->base->data->template_base->name == "function")
				{
					auto function_ptr = this->root->builder.CreateGEP(var.val, { this->root->builder.getInt32(0), this->root->builder.getInt32(0) }, "fptr");

					auto type = var.type->base->data->members.find("T")->second.ty;
					std::vector<llvm::Value*> argsv;
					for (int i = 0; i < args.size(); i++)
						argsv.push_back(this->DoCast(type->function->args[i], args[i]).val);//try and cast to the correct type if we can

					//add the data
					auto data_ptr = this->root->builder.CreateGEP(var.val, { this->root->builder.getInt32(0), this->root->builder.getInt32(1) });
					data_ptr = this->root->builder.CreateGEP(data_ptr, { this->root->builder.getInt32(0), this->root->builder.getInt32(0) });
					argsv.push_back(data_ptr);

					llvm::Value* fun = this->root->builder.CreateLoad(function_ptr);
					//fun->getType()->dump();

					auto rtype = fun->getType()->getContainedType(0)->getContainedType(0);
					std::vector<llvm::Type*> fargs;
					for (int i = 1; i < fun->getType()->getContainedType(0)->getNumContainedTypes(); i++)
						fargs.push_back(fun->getType()->getContainedType(0)->getContainedType(i));
					fargs.push_back(this->root->builder.getInt8PtrTy());

					auto fp = llvm::FunctionType::get(rtype, fargs, false)->getPointerTo();
					fun = this->root->builder.CreatePointerCast(fun, fp);
					return CValue(type->function->return_type, this->root->builder.CreateCall(fun, argsv));
				}
				else
					this->root->Error("Cannot call non-function type", *this->current_token);
			}

			if (var.type->type == Types::Pointer && var.type->base->type == Types::Function)
			{
				var.val = this->root->builder.CreateLoad(var.val);
				var.type = var.type->base;
			}

			std::vector<llvm::Value*> argsv;
			for (int i = 0; i < args.size(); i++)
				argsv.push_back(this->DoCast(var.type->function->args[i], args[i]).val);//try and cast to the correct type if we can

			return CValue(var.type->function->return_type, this->root->builder.CreateCall(var.val, argsv));
		}
		this->root->Error("Function '" + name + "' with " + std::to_string(args.size()) + " arguments is not defined", *this->current_token);
	}
	else if (fun == 0)
	{
		this->root->Error("Function '" + name + "' is not defined on object '" + Struct->ToString() + "'", *this->current_token);
	}

	fun->Load(this->root);

	if (args.size() != fun->f->arg_size())
	{
		//todo: fixme this isnt a very reliable fix
		if (fun->arguments[0].second != "this")//if we are not a constructor
			this->root->Error("Function expected " + std::to_string(fun->f->arg_size()) + " arguments, got " + std::to_string(args.size()), *this->current_token);

		//ok, we allocate, call then 
		//allocate thing
		auto type = fun->arguments[0].first->base;
		type->Load(this->root);
		auto Alloca = this->root->builder.CreateAlloca(type->GetLLVMType(), 0, "constructortemp");

		std::vector<llvm::Value*> argsv;
		int i = 1;

		//add struct
		argsv.push_back(Alloca);
		for (auto ii : args)
			argsv.push_back(this->DoCast(fun->arguments[i++].first, ii).val);//try and cast to the correct type if we can

		fun->Load(this->root);

		this->root->builder.CreateCall(fun->f, argsv);

		return CValue(type, this->root->builder.CreateLoad(Alloca));
	}

	std::vector<llvm::Value*> argsv;
	for (int i = 0; i < args.size(); i++)
		argsv.push_back(this->DoCast(fun->arguments[i].first, args[i]).val);//try and cast to the correct type if we can

	return CValue(fun->return_type, this->root->builder.CreateCall(fun->f, argsv));
}

void CompilerContext::SetDebugLocation(const Token& t)
{
	assert(this->function->loaded);
	this->root->builder.SetCurrentDebugLocation(llvm::DebugLoc::get(t.line, t.column, this->function->scope));
}


CValue CompilerContext::GetVariable(const std::string& name)
{
	auto cur = this->scope;
	CValue value;
	do
	{
		auto iter = cur->named_values.find(name);
		if (iter != cur->named_values.end())
		{
			value = iter->second;
			break;
		}
		cur = cur->prev;
	} while (cur);

	if (value.type->type == Types::Void)
	{
		//ok, now search globals
		//auto global = this->root->globals.find(name);
		//if (global != this->root->globals.end())
		//	return global->second;

		auto sym = this->root->GetVariableOrFunction(name);
		if (sym.type != SymbolType::Invalid)
		{
			if (sym.type == SymbolType::Function)// function != 0)
			{
				auto function = sym.fn;
				function->Load(this->root);
				return CValue(function->GetType(this->root), function->f);
			}
			else if (sym.type == SymbolType::Variable)
			{
				//variable
				return *sym.val;
				//throw 7;
			}
		}
		//throw 7;

		/*auto function = this->root->GetFunction(name);
		if (function != 0)
		{
			function->Load(this->root);
			return CValue(function->GetType(this->root), function->f);
		}*/

		if (this->function->is_lambda)
		{
			auto var = this->parent->GetVariable(name);

			//look in locals above me
			CValue location = this->Load("_capture_data");
			auto storage_t = this->function->lambda.storage_type;

			//append the new type
			std::vector<llvm::Type*> types;
			for (int i = 0; i < this->captures.size(); i++)
				types.push_back(storage_t->getContainedType(i));

			types.push_back(var.type->base->GetLLVMType());
			storage_t = this->function->lambda.storage_type = storage_t->create(types);

			auto data = root->builder.CreatePointerCast(location.val, storage_t->getPointerTo());

			//load it, then store it as a local
			auto val = root->builder.CreateGEP(data, { root->builder.getInt32(0), root->builder.getInt32(this->captures.size()) });

			CValue value;
			value.val = root->builder.CreateAlloca(var.type->base->GetLLVMType());
			value.type = var.type;

			this->RegisterLocal(name, value);//need to register it as immutable 

			root->builder.CreateStore(root->builder.CreateLoad(val), value.val);
			this->captures.push_back(name);

			return value;
		}

		this->root->Error("Undeclared identifier '" + name + "'", *current_token);
	}
	return value;
}

llvm::ReturnInst* CompilerContext::Return(CValue ret)
{
	//try and cast if we can
	if (this->function == 0)
		this->root->Error("Cannot return from outside function!", *current_token);

	//call destructors
	auto cur = this->scope;
	do
	{
		if (cur->destructed == false)
			for (auto ii : cur->named_values)
			{
				if (ii.second.type->type == Types::Pointer && ii.second.type->base->type == Types::Struct)
					this->Destruct(ii.second, 0);
				else if (ii.second.type->type == Types::Pointer && ii.second.type->base->type == Types::Array && ii.second.type->base->base->type == Types::Struct)
					this->Destruct(CValue(ii.second.type->base,ii.second.val), this->root->builder.getInt32(ii.second.type->base->size));
			}
		cur->destructed = true;
		cur = cur->prev;
	} while (cur);

	if (ret.val)
		ret = this->DoCast(this->function->return_type, ret);
	return root->builder.CreateRet(ret.val);
}

CValue CompilerContext::DoCast(Type* t, CValue value, bool Explicit)
{
	if (value.type->type == t->type && value.type->data == t->data)
		return value;

	llvm::Type* tt = t->GetLLVMType();// GetType(t);
	if (value.type->type == Types::Float && t->type == Types::Double)
	{
		//lets do this
		return CValue(t, root->builder.CreateFPExt(value.val, tt));
	}
	if (value.type->type == Types::Double && t->type == Types::Float)
	{
		//lets do this
		return CValue(t, root->builder.CreateFPTrunc(value.val, tt));
	}
	if (value.type->type == Types::Double || value.type->type == Types::Float)
	{
		//float to int
		if (t->type == Types::Int || t->type == Types::Short || t->type == Types::Char)
			return CValue(t, root->builder.CreateFPToSI(value.val, tt));

		//remove me later float to bool
		//if (t->type == Types::Bool)
		//return CValue(t, root->builder.CreateFCmpONE(value.val, llvm::ConstantFP::get(llvm::getGlobalContext(), llvm::APFloat(0.0))));
	}
	if (value.type->type == Types::Int || value.type->type == Types::Short || value.type->type == Types::Char)
	{
		//int to float
		if (t->type == Types::Double || t->type == Types::Float)
			return CValue(t, root->builder.CreateSIToFP(value.val, tt));
		if (t->type == Types::Bool)
			return CValue(t, root->builder.CreateIsNotNull(value.val));
		if (t->type == Types::Pointer)
		{
			//auto ty1 = value.val->getType()->getContainedType(0);
			llvm::ConstantInt* ty = llvm::dyn_cast<llvm::ConstantInt>(value.val);
			if (ty && Explicit == false && ty->getSExtValue() != 0)
				root->Error("Cannot cast a non-zero integer value to pointer implicitly.", *this->current_token);

			return CValue(t, root->builder.CreateIntToPtr(value.val, t->GetLLVMType()));
		}

		/*if (value.type->type == Types::Int && (t->type == Types::Char || t->type == Types::Short))
		{
		return CValue(t, root->builder.CreateTrunc(value.val, tt));
		}
		if (value.type->type == Types::Short && t->type == Types::Int)
		{
		return CValue(t, root->builder.CreateSExt(value.val, tt));
		}*/
		if (t->type == Types::Int || t->type == Types::Short || t->type == Types::Char)
			return CValue(t, root->builder.CreateSExtOrTrunc(value.val, tt));
	}
	if (value.type->type == Types::Pointer)
	{
		//pointer to bool
		if (t->type == Types::Bool)
			return CValue(t, root->builder.CreateIsNotNull(value.val));

		if (t->type == Types::Pointer && value.type->base->type == Types::Array && value.type->base->base == t->base)
			return CValue(t, root->builder.CreatePointerCast(value.val, t->GetLLVMType(), "arraycast"));

		if (value.type->base->type == Types::Struct && t->type == Types::Pointer && t->base->type == Types::Struct)
		{
			if (value.type->base->data->IsParent(t->base))
			{
				return CValue(t, root->builder.CreatePointerCast(value.val, t->GetLLVMType(), "ptr2ptr"));
			}
		}
		if (Explicit)
		{
			if (t->type == Types::Pointer)
			{
				//pointer to pointer cast;
				return CValue(t, root->builder.CreatePointerCast(value.val, t->GetLLVMType(), "ptr2ptr"));
			}
		}
	}
	if (value.type->type == Types::Array)
	{
		//array to pointer
		if (t->type == Types::Pointer)
		{
			if (t->base == value.type->base)
			{
				//lets just try it
				//fixme later
				//ok, this doesnt work because the value is getting loaded beforehand!!!
				/*std::vector<llvm::Value*> arr = { root->builder.getInt32(0), root->builder.getInt32(0) };

				this->f->dump();
				value.val->dump();
				value.val = root->builder.CreateGEP(value.val, arr, "array2ptr");
				value.val->dump();
				return CValue(t, value.val);*/
			}
		}
	}
	if (value.type->type == Types::Function)
	{
		if (t->type == Types::Function && t->function == value.type->function)
			return value;
		else if (Explicit && t->type == Types::Function)
			return CValue(t, this->root->builder.CreateBitOrPointerCast(value.val, tt));
	}
	if (t->type == Types::Union && value.type->type == Types::Struct)
	{
		for (int i = 0; i < t->_union->members.size(); i++)
		{
			if (t->_union->members[i] == value.type)
			{
				//Alloca the type, then store values in it and load. This is dumb but will work.
				auto alloc = this->root->builder.CreateAlloca(t->_union->type);

				//store type
				auto type = this->root->builder.CreateGEP(alloc, { this->root->builder.getInt32(0), this->root->builder.getInt32(0) });
				this->root->builder.CreateStore(this->root->builder.getInt32(i), type);

				//store data
				auto data = this->root->builder.CreateGEP(alloc, { this->root->builder.getInt32(0), this->root->builder.getInt32(1) });
				data = this->root->builder.CreatePointerCast(data, value.type->GetPointerType()->GetLLVMType());
				this->root->builder.CreateStore(value.val, data);

				//return the new value
				return CValue(t, this->root->builder.CreateLoad(alloc));
			}
		}
	}

	this->root->Error("Cannot cast '" + value.type->ToString() + "' to '" + t->ToString() + "'!", *current_token);
}


bool CompilerContext::CheckCast(Type* src, Type* t, bool Explicit, bool Throw)
{
	CValue value;
	value.type = src;

	if (value.type->type == t->type && value.type->data == t->data)
		return true;

	if (value.type->type == Types::Float && t->type == Types::Double)
	{
		//lets do this
		return true;// CValue(t, root->builder.CreateFPExt(value.val, tt));
	}
	else if (value.type->type == Types::Double && t->type == Types::Float)
	{
		//lets do this
		return true;// CValue(t, root->builder.CreateFPTrunc(value.val, tt));
	}
	else if (value.type->type == Types::Double || value.type->type == Types::Float)
	{
		//float to int
		if (t->type == Types::Int || t->type == Types::Short || t->type == Types::Char)
			return true;// CValue(t, root->builder.CreateFPToSI(value.val, tt));

		//remove me later float to bool
		//if (t->type == Types::Bool)
		//return CValue(t, root->builder.CreateFCmpONE(value.val, llvm::ConstantFP::get(llvm::getGlobalContext(), llvm::APFloat(0.0))));
	}
	if (value.type->type == Types::Int || value.type->type == Types::Short || value.type->type == Types::Char)
	{
		//int to float
		if (t->type == Types::Double || t->type == Types::Float)
			return true;// CValue(t, root->builder.CreateSIToFP(value.val, tt));
		if (t->type == Types::Bool)
			return true;// CValue(t, root->builder.CreateIsNotNull(value.val));
		if (t->type == Types::Pointer)
		{
			//auto ty1 = value.val->getType()->getContainedType(0);
			//llvm::ConstantInt* ty = llvm::dyn_cast<llvm::ConstantInt>(value.val);
			//if (ty && Explicit == false && ty->getSExtValue() != 0)
			//	return false;// root->Error("Cannot cast a non-zero integer value to pointer implicitly.", *this->current_token);

			return true;
		}

		/*if (value.type->type == Types::Int && (t->type == Types::Char || t->type == Types::Short))
		{
		return CValue(t, root->builder.CreateTrunc(value.val, tt));
		}
		if (value.type->type == Types::Short && t->type == Types::Int)
		{
		return CValue(t, root->builder.CreateSExt(value.val, tt));
		}*/
		if (t->type == Types::Int || t->type == Types::Short || t->type == Types::Char)
			return true;
	}
	if (value.type->type == Types::Pointer)
	{
		//pointer to bool
		if (t->type == Types::Bool)
			return true;

		if (t->type == Types::Pointer && value.type->base->type == Types::Array && value.type->base->base == t->base)
			return true;

		if (value.type->base->type == Types::Struct && t->type == Types::Pointer && t->base->type == Types::Struct)
		{
			if (value.type->base->data->IsParent(t->base))
			{
				return true;
			}
		}
		if (Explicit)
		{
			if (t->type == Types::Pointer)
			{
				//pointer to pointer cast;
				return true;
			}
		}
	}
	if (value.type->type == Types::Function)
	{
		if (t->type == Types::Function && t->function == value.type->function)
			return true;
		else if (Explicit && t->type == Types::Function)
			return true;
	}
	if (t->type == Types::Union && value.type->type == Types::Struct)
	{
		for (int i = 0; i < t->_union->members.size(); i++)
		{
			if (t->_union->members[i] == value.type)
				return true;
		}
	}

	//special check for traits
	if (value.type->type == Types::Trait && this->root->typecheck)
	{
		//this is a bad hack that doesnt catch all cases, but better than nothing
		auto traits = t->GetTraits(this->root);
		for (int i = 0; i < traits.size(); i++)
			if (traits[i].second->name == value.type->trait->name)// t->MatchesTrait(this->root, value.type->trait))
				return true;
	}

	//its a template arg
	if (this->root->typecheck && value.type->type == Types::Invalid)
	{
		return true;//once again, another hack that misses errors
	}

	if (Throw)
		this->root->Error("Cannot cast '" + value.type->ToString() + "' to '" + t->ToString() + "'!", *current_token);

	return false;
}

Scope* CompilerContext::PushScope()
{
	auto temp = this->scope;
	this->scope = new Scope;
	this->scope->prev = temp;

	temp->next.push_back(this->scope);
	return this->scope;
}

void CompilerContext::PopScope()
{
	//call destructors
	if (this->scope->prev != 0)// && this->scope->prev->prev != 0)
	{
		if (this->scope->destructed == false)
		for (auto ii : this->scope->named_values)
		{
			/*if (ii.second.type->type == Types::Struct)
			{
				//look for destructor
				auto name = "~" + (ii.second.type->data->template_base ? ii.second.type->data->template_base->name : ii.second.type->data->name);
				auto destructor = ii.second.type->data->functions.find(name);
				if (destructor != ii.second.type->data->functions.end())
				{
					//call it
					this->Call(name, { CValue(this->root->LookupType(ii.second.type->ToString() + "*"), ii.second.val) }, ii.second.type);
				}
			}
			else if (ii.second.type->type == Types::Array && ii.second.type->base->type == Types::Struct)
			{
				printf("ok");
			}*/

			if (ii.second.type->type == Types::Pointer && ii.second.type->base->type == Types::Struct)
				this->Destruct(ii.second, 0);
			else if (ii.second.type->type == Types::Pointer && ii.second.type->base->type == Types::Array && ii.second.type->base->base->type == Types::Struct)
				this->Destruct(CValue(ii.second.type->base, ii.second.val), this->root->builder.getInt32(ii.second.type->base->size));
		}
		this->scope->destructed = true;
	}

	auto temp = this->scope;
	this->scope = this->scope->prev;
}

void CompilerContext::WriteCaptures(llvm::Value* lambda)
{
	if (this->captures.size())
	{
		//allocate the function object
		std::vector<llvm::Type*> elements;
		for (auto ii : this->captures)
		{
			//this->CurrentToken(&ii);
			auto var = parent->GetVariable(ii);
			elements.push_back(var.type->base->GetLLVMType());
		}

		auto storage_t = llvm::StructType::get(this->root->context, elements);

		for (int i = 0; i < this->captures.size(); i++)
		{
			auto var = this->captures[i];

			//get pointer to data location
			auto ptr = root->builder.CreateGEP(lambda, { root->builder.getInt32(0), root->builder.getInt32(1) }, "lambda_data");
			ptr = root->builder.CreatePointerCast(ptr, storage_t->getPointerTo());
			ptr = root->builder.CreateGEP(ptr, { root->builder.getInt32(0), root->builder.getInt32(i) });

			//then store it
			auto val = parent->Load(var);
			root->builder.CreateStore(val.val, ptr);
		}
	}
	this->captures.clear();
}

CValue CompilerContext::Load(const std::string& name)
{
	CValue value = GetVariable(name);
	if (value.type->type == Types::Function)
		return value;
	else if (value.type->type == Types::Pointer && value.type->base->type == Types::Array)//load it as a pointer
	{
		auto type = value.type->base->GetPointerType();
		auto loc = this->root->builder.CreatePointerCast(value.val, type->GetLLVMType());
		return CValue(type, loc);
	}
	else if (value.type->type == Types::Int)
	{
		//its a constant
		return value;
	}
	return CValue(value.type->base, root->builder.CreateLoad(value.val, name.c_str()));
}

void CompilerContext::Construct(CValue pointer, llvm::Value* arr_size)
{
	if (pointer.type->base->type == Types::Struct)
	{
		Type* ty = pointer.type->base;
		Function* fun = 0;
		if (ty->data->template_base)
			fun = ty->GetMethod(ty->data->template_base->name, { pointer.type }, this);
		else
			fun = ty->GetMethod(ty->data->name, { pointer.type }, this);
		fun->Load(this->root);
		if (arr_size == 0)//size == 0)
		{//just one element, construct it
			this->root->builder.CreateCall(fun->f, { pointer.val });
		}
		else
		{//construct each child element
			llvm::Value* counter = this->root->builder.CreateAlloca(this->root->IntType->GetLLVMType(), 0, "newcounter");
			this->root->builder.CreateStore(this->Integer(0).val, counter);

			auto start = llvm::BasicBlock::Create(this->root->context, "start", this->root->current_function->function->f);
			auto body = llvm::BasicBlock::Create(this->root->context, "body", this->root->current_function->function->f);
			auto end = llvm::BasicBlock::Create(this->root->context, "end", this->root->current_function->function->f);

			this->root->builder.CreateBr(start);
			this->root->builder.SetInsertPoint(start);
			auto cval = this->root->builder.CreateLoad(counter, "curcount");
			auto res = this->root->builder.CreateICmpUGE(cval, arr_size);
			this->root->builder.CreateCondBr(res, end, body);

			this->root->builder.SetInsertPoint(body);
			if (pointer.type->type == Types::Array)
			{
				auto elementptr = this->root->builder.CreateGEP(pointer.val, { this->root->builder.getInt32(0), cval });
				this->root->builder.CreateCall(fun->f, { elementptr });
			}
			else
			{
				auto elementptr = this->root->builder.CreateGEP(pointer.val, { cval });
				this->root->builder.CreateCall(fun->f, { elementptr });
			}

			auto inc = this->root->builder.CreateAdd(cval, this->Integer(1).val);
			this->root->builder.CreateStore(inc, counter);

			this->root->builder.CreateBr(start);

			this->root->builder.SetInsertPoint(end);
		}
	}
}

void CompilerContext::Destruct(CValue pointer, llvm::Value* arr_size)
{
	if (pointer.type->base->type == Types::Struct)
	{
		Type* ty = pointer.type->base;
		Function* fun = 0;
		if (ty->data->template_base)
			fun = ty->GetMethod("~"+ty->data->template_base->name, { pointer.type }, this);
		else
			fun = ty->GetMethod("~"+ty->data->name, { pointer.type }, this);
		if (fun == 0)
			return;
		fun->Load(this->root);
		if (arr_size == 0)//size == 0)
		{//just one element, construct it
			this->root->builder.CreateCall(fun->f, { pointer.val });
		}
		else
		{//construct each child element
			llvm::Value* counter = this->root->builder.CreateAlloca(this->root->IntType->GetLLVMType(), 0, "newcounter");
			this->root->builder.CreateStore(this->Integer(0).val, counter);

			auto start = llvm::BasicBlock::Create(this->root->context, "start", this->root->current_function->function->f);
			auto body = llvm::BasicBlock::Create(this->root->context, "body", this->root->current_function->function->f);
			auto end = llvm::BasicBlock::Create(this->root->context, "end", this->root->current_function->function->f);

			this->root->builder.CreateBr(start);
			this->root->builder.SetInsertPoint(start);
			auto cval = this->root->builder.CreateLoad(counter, "curcount");
			auto res = this->root->builder.CreateICmpUGE(cval, arr_size);
			this->root->builder.CreateCondBr(res, end, body);

			this->root->builder.SetInsertPoint(body);
			if (pointer.type->type == Types::Array)
			{
				auto elementptr = this->root->builder.CreateGEP(pointer.val, { this->root->builder.getInt32(0), cval });
				this->root->builder.CreateCall(fun->f, { elementptr });
			}
			else
			{
				auto elementptr = this->root->builder.CreateGEP(pointer.val, { cval });
				this->root->builder.CreateCall(fun->f, { elementptr });
			}

			auto inc = this->root->builder.CreateAdd(cval, this->Integer(1).val);
			this->root->builder.CreateStore(inc, counter);

			this->root->builder.CreateBr(start);

			this->root->builder.SetInsertPoint(end);
		}
	}
}