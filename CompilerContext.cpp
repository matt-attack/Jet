#include "Compiler.h"
#include "CompilerContext.h"

#include "Lexer.h"
#include "Expressions.h"

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
			if (ii->second->arguments.size() == args.size())
				func = ii->second;
		}
	}
	if (iter == parent->functions.end() && member == false)
	{
		//no function exists
		func = new Function(fname);
		func->return_type = ret;
		func->arguments = args;
		std::vector<llvm::Type*> oargs;
		std::vector<llvm::Metadata*> ftypes;
		for (int i = 0; i < args.size(); i++)
		{
			oargs.push_back(GetType(args[i].first));
			ftypes.push_back(args[i].first->GetDebugType(this->parent));
		}

		auto n = new CompilerContext(this->parent);

		auto ft = llvm::FunctionType::get(GetType(ret), oargs, false);

		func->f = n->f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, fname, parent->module);

		n->function = func;

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
			if (ii->second->arguments.size() == args.size())
				func = ii->second;
		}

		if (func == 0)
			this->parent->Error("Function '" + fname + "' not found", *this->parent->current_function->current_token);
	}

	func->Load(this->parent);

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
		assert(false);
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
			this->parent->Error("Invalid Unary Operation '" + TokenToString[operation] + "' On Type '" + value.type->base->ToString() + "'", *current_token);
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
		parent->Error("Cannot perform a binary operation between two incompatible types", *this->current_token);
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
			res = parent->builder.CreateShl(left.val, right.val);
			break;
		case TokenType::RightShift:
			//todo
			res = parent->builder.CreateLShr(left.val, right.val);
			break;
		default:
			this->parent->Error("Invalid Binary Operation '" + TokenToString[op] + "' On Type '" + left.type->ToString() + "'", *current_token);

			break;
		}

		return CValue(left.type, res);
	}

	this->parent->Error("Invalid Binary Operation '" + TokenToString[op] + "' On Type '" + left.type->ToString() + "'", *current_token);
}

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
				type->second->Load(this->parent);
				//look for a constructor
				auto range = type->second->data->functions.equal_range(name);
				for (auto ii = range.first; ii != range.second; ii++)
				{
					if (ii->second->arguments.size() == args.size() + 1)
						fun = ii->second;
				}
				if (fun)
				{
					//ok, we allocate, call then 
					//allocate thing
					//auto Alloca = this->parent->builder.CreateAlloca(GetType(type->second), 0, "constructortemp");

					//fun->Load(this->parent);

					return 0;// fun;// CValue(type->second, this->parent->builder.CreateLoad(Alloca));
				}
			}
		}

		if (name[name.length() - 1] == '>')//its a template
		{
			int i = name.find_first_of('<');
			auto base_name = name.substr(0, i);

			auto range = this->parent->functions.equal_range(name);

			//instantiate here
			this->parent->Error("Not implemented", *this->current_token);

			return range.first->second;
		}

		//look for the best one
		auto range = this->parent->functions.equal_range(name);
		for (auto ii = range.first; ii != range.second; ii++)
		{
			//pick one with the right number of args
			if (ii->second->arguments.size() == args.size())
				fun = ii->second;
		}

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
							if (templates[i] != 0 && templates[i] != args[i2].type)
								this->parent->Error("Could not infer template type", *this->current_token);

							templates[i] = args[i2].type;
						}
						i2++;
					}
					i++;
				}
			}
			
			for (int i = 0; i < fun->templates.size(); i++)
			{
				if (templates[i] == 0)
					this->parent->Error("Could not infer template type", *this->current_token);
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
			std::vector<std::pair<std::string, Type*>> old;
			for (auto ii : fun->templates)
			{
				//check if traits match
				if (templates[i]->MatchesTrait(this->parent, ii.first->trait) == false)
					parent->Error("Type '" + templates[i]->name + "' doesn't match Trait '" + ii.first->name + "'", *parent->current_function->current_token);

				old.push_back({ ii.second, parent->types[ii.second] });
				parent->types[ii.second] = templates[i++];
			}

			//store then restore insertion point
			auto rp = parent->builder.GetInsertBlock();
			auto dp = parent->builder.getCurrentDebugLocation();


			fun->expression->CompileDeclarations(this);
			fun->expression->DoCompile(this);

			//restore types

			for (auto ii : old)
			{
				//need to remove everything that is using me
				if (ii.second)
					parent->types[ii.first] = ii.second;
				else
					parent->types.erase(parent->types.find(ii.first));

				//need to get rid of all references to it, or blam
				auto iter = parent->types.find(ii.first + "*");
				if (iter != parent->types.end())
				{
					parent->types.erase(iter);

					iter = parent->types.find(ii.first + "**");
					if (iter != parent->types.end())
						parent->types.erase(iter);
				}
			}

			parent->builder.SetCurrentDebugLocation(dp);
			if (rp)
				parent->builder.SetInsertPoint(rp);

			fun->expression->name.text = oldname;

			//time to recompile and stuff
			return parent->functions.find(rname)->second;
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
				if (ii->second->arguments.size() == args.size() + 1)
					fun = ii->second;
			}
			if (fun)
			{
				//ok, we allocate, call then 
				//allocate thing
				type->second->Load(this->parent);
				auto Alloca = this->parent->builder.CreateAlloca(GetType(type->second), 0, "constructortemp");

				std::vector<llvm::Value*> argsv;
				int i = 1;

				//add struct
				argsv.push_back(Alloca);
				for (auto ii : args)
					argsv.push_back(this->DoCast(fun->arguments[i++].first, ii).val);//try and cast to the correct type if we can

				fun->Load(this->parent);

				this->parent->builder.CreateCall(fun->f, argsv);

				return CValue(type->second, this->parent->builder.CreateLoad(Alloca));
			}
		}
		else
		{
			//try to find it in variables
			auto var = this->Load(name);

			if (var.type->type != Types::Function)
				this->parent->Error("Cannot call non-function type", *this->current_token);

			std::vector<llvm::Value*> argsv;
			for (int i = 0; i < args.size(); i++)
				argsv.push_back(this->DoCast(var.type->function->args[i], args[i]).val);//try and cast to the correct type if we can

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

	if (args.size() != fun->f->arg_size())
		this->parent->Error("Mismatched function parameters in call", *this->current_token);//todo: add better checks later

	std::vector<llvm::Value*> argsv;
	for (int i = 0; i < args.size(); i++)// auto ii : args)
		argsv.push_back(this->DoCast(fun->arguments[i].first, args[i]).val);//try and cast to the correct type if we can

	return CValue(fun->return_type, this->parent->builder.CreateCall(fun->f, argsv));
}