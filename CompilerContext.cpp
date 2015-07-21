#include "Compiler.h"
#include "CompilerContext.h"


#include "Lexer.h"

using namespace Jet;

CompilerContext* CompilerContext::AddFunction(const std::string& fname, Type* ret, const std::vector<std::pair<Type*, std::string>>& args, bool member)
{
	auto iter = parent->functions.find(fname);
	Function* func = 0;
	if (member)
	{
		std::string str;
		int i = 2;
		for (; i < fname.length(); i++)
		{
			if (fname[i] == '_')
				break;

			str += fname[i];
		}
		auto type = this->parent->LookupType(str);

		std::string fname2 = fname.substr(++i, fname.length() - i);

		auto range = type->data->functions.equal_range(fname2);
		for (auto ii = range.first; ii != range.second; ii++)
		{
			//printf("found option for %s with %i args\n", fname.c_str(), ii->second->argst.size());
			if (ii->second->argst.size() == args.size())
				func = ii->second;
		}
	}
	if (iter == parent->functions.end() && member == false)
	{
		//no function exists
		func = new Function;
		func->return_type = ret;
		func->argst = args;
		func->name = fname;
		std::vector<llvm::Type*> oargs;
		std::vector<llvm::Metadata*> ftypes;
		for (int i = 0; i < args.size(); i++)
		{
			oargs.push_back(GetType(args[i].first));
			ftypes.push_back(args[i].first->GetDebugType(this->parent));
		}

		auto n = new CompilerContext(this->parent);

		auto ft = llvm::FunctionType::get(GetType(ret), oargs, false);

		n->f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, fname, parent->module);

		n->function = func;
		func->f = n->f;
		if (member == false)
			this->parent->functions.insert({ fname, func });// [fname] = func;

		llvm::DIFile unit = parent->debug_info.file;

		auto functiontype = parent->debug->createSubroutineType(unit, parent->debug->getOrCreateTypeArray(ftypes));
		llvm::DISubprogram sp = parent->debug->createFunction(unit, fname, fname, unit, 0, functiontype, false, true, 0, 0, false, n->f);

		assert(sp.describes(n->f));
		func->scope = sp;
		parent->builder.SetCurrentDebugLocation(llvm::DebugLoc::get(5, 1, 0));

		llvm::BasicBlock *bb = llvm::BasicBlock::Create(parent->context, "entry", n->f);
		parent->builder.SetInsertPoint(bb);
		return n;
	}
	else if (func == 0)
	{
		//select the right one
		auto range = parent->functions.equal_range(fname);
		for (auto ii = range.first; ii != range.second; ii++)
		{
			//printf("found option for %s with %i args\n", fname.c_str(), ii->second->argst.size());
			if (ii->second->argst.size() == args.size())
				func = ii->second;
		}

		if (func == 0)
			this->parent->Error("Function '" + fname + "' not found", *this->parent->current_function->current_token);
	}

	func->Load(this->parent);

	//func->scope->dump();
	auto n = new CompilerContext(this->parent);
	n->f = func->f;
	n->function = func;

	llvm::BasicBlock *bb = llvm::BasicBlock::Create(parent->context, "entry", n->f);
	parent->builder.SetInsertPoint(bb);

	this->parent->current_function = n;
	return n;
}

CValue CompilerContext::UnaryOperation(TokenType operation, CValue value)
{
	llvm::Value* res = 0;

	if (operation == TokenType::BAnd)
	{
		//this should already have been done elsewhere, so error
		throw 7;
	}

	if (value.type->type == Types::Float || value.type->type == Types::Double)
	{
		switch (operation)
		{
			/*case TokenType::Increment:
				parent->builder.create
				throw 7;
				//res = parent->builder.CreateFAdd(left.val, right.val);
				break;
				case TokenType::Decrement:
				throw 7;
				//res = parent->builder.CreateFSub(left.val, right.val);
				break;*/
		case TokenType::Minus:
			res = parent->builder.CreateFNeg(value.val);// parent->builder.CreateFMul(left.val, right.val);
			break;
		default:
			this->parent->Error("Invalid Unary Operation '" + TokenToString[operation] + "' On Type '" + value.type->ToString() + "'", *current_token);
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
			res = parent->builder.CreateAdd(value.val, parent->builder.getInt32(1));
			break;
		case TokenType::Decrement:
			res = parent->builder.CreateSub(value.val, parent->builder.getInt32(1));
			break;
		case TokenType::Minus:
			res = parent->builder.CreateNeg(value.val);
			break;
		case TokenType::BNot:
			res = parent->builder.CreateNot(value.val);
			break;
		default:
			this->parent->Error("Invalid Unary Operation '" + TokenToString[operation] + "' On Type '" + value.type->ToString() + "'", *current_token);
			break;
		}

		return CValue(value.type, res);
	}
	else if (value.type->type == Types::Pointer)
	{
		switch (operation)
		{
		case TokenType::Asterisk:
			return CValue(value.type->base, this->parent->builder.CreateLoad(value.val));
		case TokenType::Increment:
			return CValue(value.type->base, this->parent->builder.CreateGEP(value.val, parent->builder.getInt32(1)));
		case TokenType::Decrement:
			return CValue(value.type->base, this->parent->builder.CreateGEP(value.val, parent->builder.getInt32(-1)));
		default:
			this->parent->Error("Invalid Unary Operation '" + TokenToString[operation] + "' On Type '" + value.type->ToString() + "'", *current_token);
		}

	}
	this->parent->Error("Invalid Unary Operation '" + TokenToString[operation] + "' On Type '" + value.type->ToString() + "'", *current_token);
}

CValue CompilerContext::BinaryOperation(Jet::TokenType op, CValue left, CValue right)
{
	llvm::Value* res = 0;

	//should this be a floating point calc?
	if (left.type->type != right.type->type)
	{
		//conversion time!!

		throw 7;
	}

	if (left.type->type == Types::Float || left.type->type == Types::Double)
	{
		switch (op)
		{
		case TokenType::AddAssign:
		case TokenType::Plus:
			res = parent->builder.CreateFAdd(left.val, right.val);
			break;
		case TokenType::SubtractAssign:
		case TokenType::Minus:
			res = parent->builder.CreateFSub(left.val, right.val);
			break;
		case TokenType::MultiplyAssign:
		case TokenType::Asterisk:
			res = parent->builder.CreateFMul(left.val, right.val);
			break;
		case TokenType::DivideAssign:
		case TokenType::Slash:
			res = parent->builder.CreateFDiv(left.val, right.val);
			break;
		case TokenType::LessThan:
			//use U or O?
			res = parent->builder.CreateFCmpULT(left.val, right.val);
			return CValue(parent->BoolType, res);
			break;
		case TokenType::LessThanEqual:
			res = parent->builder.CreateFCmpULE(left.val, right.val);
			return CValue(parent->BoolType, res);
			break;
		case TokenType::GreaterThan:
			res = parent->builder.CreateFCmpUGT(left.val, right.val);
			return CValue(parent->BoolType, res);
			break;
		case TokenType::GreaterThanEqual:
			res = parent->builder.CreateFCmpUGE(left.val, right.val);
			return CValue(parent->BoolType, res);
			break;
		case TokenType::Equals:
			res = parent->builder.CreateFCmpUEQ(left.val, right.val);
			return CValue(parent->BoolType, res);
			break;
		case TokenType::NotEqual:
			res = parent->builder.CreateFCmpUNE(left.val, right.val);
			return CValue(parent->BoolType, res);
			break;
		default:
			this->parent->Error("Invalid Binary Operation '" + TokenToString[op] + "' On Type '" + left.type->ToString() + "'", *current_token);

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
			res = parent->builder.CreateAdd(left.val, right.val);
			break;
		case TokenType::SubtractAssign:
		case TokenType::Minus:
			res = parent->builder.CreateSub(left.val, right.val);
			break;
		case TokenType::MultiplyAssign:
		case TokenType::Asterisk:
			res = parent->builder.CreateMul(left.val, right.val);
			break;
		case TokenType::DivideAssign:
		case TokenType::Slash:
			if (true)//signed
				res = parent->builder.CreateSDiv(left.val, right.val);
			else//unsigned
				res = parent->builder.CreateUDiv(left.val, right.val);
			break;
		case TokenType::Modulo:
			if (true)//signed
				res = parent->builder.CreateSRem(left.val, right.val);
			else//unsigned
				res = parent->builder.CreateURem(left.val, right.val);
			break;
			//todo add unsigned
		case TokenType::LessThan:
			//use U or S?
			res = parent->builder.CreateICmpSLT(left.val, right.val);
			return CValue(parent->BoolType, res);
			break;
		case TokenType::LessThanEqual:
			res = parent->builder.CreateICmpSLE(left.val, right.val);
			return CValue(parent->BoolType, res);
			break;
		case TokenType::GreaterThan:
			res = parent->builder.CreateICmpSGT(left.val, right.val);
			return CValue(parent->BoolType, res);
			break;
		case TokenType::GreaterThanEqual:
			res = parent->builder.CreateICmpSGE(left.val, right.val);
			return CValue(parent->BoolType, res);
			break;
		case TokenType::Equals:
			res = parent->builder.CreateICmpEQ(left.val, right.val);
			return CValue(parent->BoolType, res);
			break;
		case TokenType::NotEqual:
			res = parent->builder.CreateICmpNE(left.val, right.val);
			return CValue(parent->BoolType, res);
			break;
		case TokenType::BAnd:
		case TokenType::AndAssign:
			res = parent->builder.CreateAnd(left.val, right.val);
			break;
		case TokenType::BOr:
		case TokenType::OrAssign:
			res = parent->builder.CreateOr(left.val, right.val);
			break;
		case TokenType::Xor:
		case TokenType::XorAssign:
			res = parent->builder.CreateXor(left.val, right.val);
			break;
		case TokenType::LeftShift:
			//todo
		case TokenType::RightShift:
			//todo
		default:
			this->parent->Error("Invalid Binary Operation '" + TokenToString[op] + "' On Type '" + left.type->ToString() + "'", *current_token);

			break;
		}

		return CValue(left.type, res);
	}

	this->parent->Error("Invalid Binary Operation '" + TokenToString[op] + "' On Type '" + left.type->ToString() + "'", *current_token);
}

#include "Expressions.h"

Function* CompilerContext::GetMethod(const std::string& name, const std::vector<CValue>& args, Type* Struct)
{
	Function* fun = 0;

	if (Struct == 0)
	{
		//global function?
		auto iter = this->parent->functions.find(name);
		if (iter == this->parent->functions.end())
		{
			//check if its a type, if so try and find a constructor
			auto type = this->parent->types.find(name);// LookupType(name);
			if (type != this->parent->types.end() && type->second->type == Types::Struct)
			{
				//look for a constructor
				auto range = type->second->data->functions.equal_range(name);
				for (auto ii = range.first; ii != range.second; ii++)
				{
					if (ii->second->argst.size() == args.size() + 1)
						fun = ii->second;
				}
				if (fun)//constructor != type->data->functions.end() && constructor->second->argst.size() == args.size() + 1)
				{
					//ok, we allocate, call then 
					//allocate thing
					auto Alloca = this->parent->builder.CreateAlloca(GetType(type->second), 0, "constructortemp");

					std::vector<llvm::Value*> argsv;
					int i = 1;

					//add struct
					argsv.push_back(Alloca);
					for (auto ii : args)
					{
						//try and cast to the correct type if we can
						argsv.push_back(this->DoCast(fun->argst[i++].first, ii).val);
						//argsv.back()->dump();
					}

					fun->Load(this->parent);

					//constructor->second->f->dump();
					//parent->module->dump();
					//auto fun = this->parent->module->getFunction(constructor->second->name);

					//this->parent->builder.CreateCall(fun->f, argsv);

					return fun;// CValue(type->second, this->parent->builder.CreateLoad(Alloca));
				}
			}
			//else if (type == this->parent->types.end() || type->second->type == Types::Invalid)
			//{
			//Error("wrong something", *current_token);
			//}
			/*else
			{
			//try to find it in variables
			auto var = this->GetVariable(name);

			if (var.type->type != Types::Function)
			this->parent->Error("Cannot call non-function type", *this->current_token);

			std::vector<llvm::Value*> argsv;
			int i = 0;
			for (auto ii : args)
			{
			//try and cast to the correct type if we can
			argsv.push_back(this->DoCast(var.type->function->args[i++], ii).val);
			//argsv.back()->dump();
			}

			//var.val->dump();
			var.val = this->parent->builder.CreateLoad(var.val);
			return CValue(var.type->function->return_type, this->parent->builder.CreateCall(var.val, argsv));
			}*/
			//this->parent->Error("Function '" + name + "' is not defined", *this->current_token);
		}

		//look for the best one
		auto range = this->parent->functions.equal_range(name);
		for (auto ii = range.first; ii != range.second; ii++)
		{
			//printf("found function option for %s\n", name.c_str());

			//pick one with the right number of args
			if (ii->second->argst.size() == args.size())
				fun = ii->second;
		}

		//if (fun == 0)
		//this->parent->Error("Mismatched function parameters in call", *this->current_token);
		//fun = iter->second;
	}
	else
	{
		fun = Struct->GetMethod(name, args, this);

		//im a struct yo
		//if (fun == 0)
		//this->parent->Error("Function '" + name + "' is not defined on object '" + Struct->ToString() + "'", *this->current_token);
	}
	return fun;
}

CValue CompilerContext::Call(const std::string& name, const std::vector<CValue>& args, Type* Struct)
{
	llvm::Function* f = 0;
	Function* fun = this->GetMethod(name, args, Struct);

	if (fun == 0 && Struct == 0)
	{
		//global function?
		//check if its a type, if so try and find a constructor
		auto type = this->parent->types.find(name);// LookupType(name);
		if (type != this->parent->types.end() && type->second->type == Types::Struct)
		{
			//look for a constructor
			auto range = type->second->data->functions.equal_range(name);
			for (auto ii = range.first; ii != range.second; ii++)
			{
				if (ii->second->argst.size() == args.size() + 1)
					fun = ii->second;
			}
			if (fun)//constructor != type->data->functions.end() && constructor->second->argst.size() == args.size() + 1)
			{
				//ok, we allocate, call then 
				//allocate thing
				auto Alloca = this->parent->builder.CreateAlloca(GetType(type->second), 0, "constructortemp");

				std::vector<llvm::Value*> argsv;
				int i = 1;

				//add struct
				argsv.push_back(Alloca);
				for (auto ii : args)
				{
					//try and cast to the correct type if we can
					argsv.push_back(this->DoCast(fun->argst[i++].first, ii).val);
					//argsv.back()->dump();
				}

				fun->Load(this->parent);

				//constructor->second->f->dump();
				//parent->module->dump();
				//auto fun = this->parent->module->getFunction(constructor->second->name);

				this->parent->builder.CreateCall(fun->f, argsv);

				return CValue(type->second, this->parent->builder.CreateLoad(Alloca));
			}
		}
		else
		{
			//try to find it in variables
			auto var = this->GetVariable(name);

			if (var.type->type != Types::Function)
				this->parent->Error("Cannot call non-function type", *this->current_token);

			std::vector<llvm::Value*> argsv;
			int i = 0;
			for (auto ii : args)
			{
				//try and cast to the correct type if we can
				argsv.push_back(this->DoCast(var.type->function->args[i++], ii).val);
				//argsv.back()->dump();
			}

			//var.val->dump();
			var.val = this->parent->builder.CreateLoad(var.val);
			return CValue(var.type->function->return_type, this->parent->builder.CreateCall(var.val, argsv));
		}
		this->parent->Error("Function '" + name + "' is not defined", *this->current_token);
	}
	else
	{
		//im a struct yo
		if (fun == 0)
			this->parent->Error("Function '" + name + "' is not defined on object '" + Struct->ToString() + "'", *this->current_token);
	}

	fun->Load(this->parent);

	f = fun->f;// this->parent->module->getFunction(fun->name);

	if (args.size() != f->arg_size())
	{
		//f->dump();
		//todo: add better checks later
		this->parent->Error("Mismatched function parameters in call", *this->current_token);
	}

	std::vector<llvm::Value*> argsv;
	int i = 0;
	for (auto ii : args)
	{
		//try and cast to the correct type if we can
		argsv.push_back(this->DoCast(fun->argst[i++].first, ii).val);
		//argsv.back()->dump();
	}
	//fun->f->dump();
	//parent->module->dump();
	return CValue(fun->return_type, this->parent->builder.CreateCall(f, argsv));
}