#include "Compiler.h"
#include "CompilerContext.h"
#include "Types/Function.h"

#include "Lexer.h"
#include "Expressions.h"

using namespace Jet;

CompilerContext* CompilerContext::AddFunction(const std::string& fname, Type* ret, const std::vector<std::pair<Type*, std::string>>& args, bool member)
{
	auto iter = parent->ns->GetFunction(fname);// functions.find(fname);
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
	if (iter == 0 && member == false)
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
		this->parent->functions.push_back(n->f);
		n->function = func;

		if (member == false)
			this->parent->ns->members.insert({ fname, func });// [fname] = func;

		llvm::DIFile* unit = parent->debug_info.file;

		auto functiontype = parent->debug->createSubroutineType(unit, parent->debug->getOrCreateTypeArray(ftypes));
		llvm::DISubprogram* sp = parent->debug->createFunction(unit, fname, fname, unit, 0, functiontype, false, true, 0, 0, false, n->f);

		assert(sp->describes(n->f));
		func->scope = sp;
		parent->builder.SetCurrentDebugLocation(llvm::DebugLoc::get(5, 1, 0));

		llvm::BasicBlock *bb = llvm::BasicBlock::Create(parent->context, "entry", n->f);
		parent->builder.SetInsertPoint(bb);
		return n;
	}
	else if (func == 0)
	{
		//select the right one
		func = parent->ns->GetFunction(fname/*, args*/);// functions.equal_range(fname);

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
		auto iter = this->parent->GetFunction(name);// functions.find(name);
		if (iter == 0)//this->parent->functions.end())
		{
			//check if its a type, if so try and find a constructor
			auto type = this->parent->TryLookupType(name);// types.find(name);// LookupType(name);
			if (type != 0 && type->type == Types::Struct)
			{
				type->Load(this->parent);
				//look for a constructor
				auto range = type->data->functions.equal_range(name);
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

			//auto range = this->parent->functions.equal_range(name);

			//instantiate here
			this->parent->Error("Not implemented", *this->current_token);

			//return range.first->second;
		}

		//look for the best one
		fun = this->parent->GetFunction(name, args);// functions.equal_range(name);

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
			for (auto ii : fun->templates)
			{
				//check if traits match
				if (templates[i]->MatchesTrait(this->parent, ii.first->trait) == false)
					parent->Error("Type '" + templates[i]->name + "' doesn't match Trait '" + ii.first->name + "'", *parent->current_function->current_token);

				parent->ns->members.insert({ ii.second, templates[i++] });
			}

			//store then restore insertion point
			auto rp = parent->builder.GetInsertBlock();
			auto dp = parent->builder.getCurrentDebugLocation();


			fun->expression->CompileDeclarations(this);
			fun->expression->DoCompile(this);

			parent->builder.SetCurrentDebugLocation(dp);
			if (rp)
				parent->builder.SetInsertPoint(rp);

			fun->expression->name.text = oldname;

			//time to recompile and stuff
			return parent->ns->members.find(rname)->second.fn;
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
		auto type = this->parent->TryLookupType(name);
		if (type != 0 && type->type == Types::Struct)
		{
			//look for a constructor
			auto range = type->data->functions.equal_range(name);
			for (auto ii = range.first; ii != range.second; ii++)
			{
				if (ii->second->arguments.size() == args.size() + 1)
					fun = ii->second;
			}
			if (fun)
			{
				//ok, we allocate, call then 
				//allocate thing
				type->Load(this->parent);
				auto Alloca = this->parent->builder.CreateAlloca(GetType(type), 0, "constructortemp");

				std::vector<llvm::Value*> argsv;
				int i = 1;

				//add struct
				argsv.push_back(Alloca);
				for (auto ii : args)
					argsv.push_back(this->DoCast(fun->arguments[i++].first, ii).val);//try and cast to the correct type if we can

				fun->Load(this->parent);

				this->parent->builder.CreateCall(fun->f, argsv);

				return CValue(type, this->parent->builder.CreateLoad(Alloca));
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
					auto function_ptr = this->parent->builder.CreateGEP(var.val, { this->parent->builder.getInt32(0), this->parent->builder.getInt32(0) }, "fptr");

					auto type = var.type->base->data->members.find("T")->second.ty;
					std::vector<llvm::Value*> argsv;
					for (int i = 0; i < args.size(); i++)
						argsv.push_back(this->DoCast(type->function->args[i], args[i]).val);//try and cast to the correct type if we can

					//add the data
					auto data_ptr = this->parent->builder.CreateGEP(var.val, { this->parent->builder.getInt32(0), this->parent->builder.getInt32(1) });
					data_ptr = this->parent->builder.CreateGEP(data_ptr, { this->parent->builder.getInt32(0), this->parent->builder.getInt32(0) });
					argsv.push_back(data_ptr);

					llvm::Value* fun = this->parent->builder.CreateLoad(function_ptr);
					//fun->getType()->dump();

					auto rtype = fun->getType()->getContainedType(0)->getContainedType(0);
					std::vector<llvm::Type*> fargs;
					for (int i = 1; i < fun->getType()->getContainedType(0)->getNumContainedTypes(); i++)
						fargs.push_back(fun->getType()->getContainedType(0)->getContainedType(i));
					fargs.push_back(this->parent->builder.getInt8PtrTy());

					auto fp = llvm::FunctionType::get(rtype, fargs, false)->getPointerTo();
					fun = this->parent->builder.CreatePointerCast(fun, fp);
					return CValue(type->function->return_type, this->parent->builder.CreateCall(fun, argsv));
				}
				else
					this->parent->Error("Cannot call non-function type", *this->current_token);
			}

			if (var.type->type == Types::Pointer && var.type->base->type == Types::Function)
			{
				var.val = this->parent->builder.CreateLoad(var.val);
				var.type = var.type->base;
			}

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
	{
		//this->DoCast(fun->arguments[i].first, args[i]).val->dump();
		argsv.push_back(this->DoCast(fun->arguments[i].first, args[i]).val);//try and cast to the correct type if we can
	}

	//fun->f->dump();
	return CValue(fun->return_type, this->parent->builder.CreateCall(fun->f, argsv));
}

void CompilerContext::SetDebugLocation(const Token& t)
{
	assert(this->function->loaded);
	this->parent->builder.SetCurrentDebugLocation(llvm::DebugLoc::get(t.line, t.column, this->function->scope));
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
		auto global = this->parent->globals.find(name);
		if (global != this->parent->globals.end())
			return global->second;

		auto function = this->parent->GetFunction(name);//this->parent->functions.find(name);
		if (function != 0)//this->parent->functions.end())
		{
			function->Load(this->parent);
			return CValue(function->GetType(this->parent), function->f);
		}

		this->parent->Error("Undeclared identifier '" + name + "'", *current_token);
	}
	return value;
}

llvm::ReturnInst* CompilerContext::Return(CValue ret)
{
	//try and cast if we can
	if (this->function == 0)
		this->parent->Error("Cannot return from outside function!", *current_token);

	//call destructors
	auto cur = this->scope;
	do
	{
		for (auto ii : cur->named_values)
		{
			if (ii.second.type->type == Types::Struct)
			{
				//look for destructor
				auto name = "~" + (ii.second.type->data->template_base ? ii.second.type->data->template_base->name : ii.second.type->data->name);
				auto destructor = ii.second.type->data->functions.find(name);
				if (destructor != ii.second.type->data->functions.end())
				{
					//call it
					this->Call(name, { CValue(this->parent->LookupType(ii.second.type->ToString() + "*"), ii.second.val) }, ii.second.type);
				}
			}
		}
		cur = cur->prev;
	} while (cur);

	if (ret.val)
		ret = this->DoCast(this->function->return_type, ret);
	return parent->builder.CreateRet(ret.val);
}

CValue CompilerContext::DoCast(Type* t, CValue value, bool Explicit)
{
	if (value.type->type == t->type && value.type->data == t->data)
		return value;

	llvm::Type* tt = GetType(t);
	if (value.type->type == Types::Float && t->type == Types::Double)
	{
		//lets do this
		return CValue(t, parent->builder.CreateFPExt(value.val, tt));
	}
	if (value.type->type == Types::Double && t->type == Types::Float)
	{
		//lets do this
		return CValue(t, parent->builder.CreateFPTrunc(value.val, tt));
	}
	if (value.type->type == Types::Double || value.type->type == Types::Float)
	{
		//float to int
		if (t->type == Types::Int || t->type == Types::Short || t->type == Types::Char)
			return CValue(t, parent->builder.CreateFPToSI(value.val, tt));

		//remove me later float to bool
		if (t->type == Types::Bool)
			return CValue(t, parent->builder.CreateFCmpONE(value.val, llvm::ConstantFP::get(llvm::getGlobalContext(), llvm::APFloat(0.0))));
	}
	if (value.type->type == Types::Int || value.type->type == Types::Short || value.type->type == Types::Char)
	{
		//int to float
		if (t->type == Types::Double || t->type == Types::Float)
			return CValue(t, parent->builder.CreateSIToFP(value.val, tt));
		if (t->type == Types::Bool)
			return CValue(t, parent->builder.CreateIsNotNull(value.val));
		if (t->type == Types::Pointer)
		{
			return CValue(t, parent->builder.CreateIntToPtr(value.val, GetType(t)));
		}

		if (value.type->type == Types::Int && (t->type == Types::Char || t->type == Types::Short))
		{
			return CValue(t, parent->builder.CreateTrunc(value.val, GetType(t)));
		}
	}
	if (value.type->type == Types::Pointer)
	{
		//pointer to bool
		if (t->type == Types::Bool)
			return CValue(t, parent->builder.CreateIsNotNull(value.val));
		if (Explicit)
		{
			if (t->type == Types::Pointer)
			{
				//pointer to pointer cast;
				return CValue(t, parent->builder.CreatePointerCast(value.val, GetType(t), "ptr2ptr"));
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
				/*std::vector<llvm::Value*> arr = { parent->builder.getInt32(0), parent->builder.getInt32(0) };

				this->f->dump();
				value.val->dump();
				value.val = parent->builder.CreateGEP(value.val, arr, "array2ptr");
				value.val->dump();
				return CValue(t, value.val);*/
			}
		}
	}
	if (value.type->type == Types::Function)
	{
		if (t->type == Types::Function && t->function == value.type->function)
			return value;
	}

	this->parent->Error("Cannot cast '" + value.type->ToString() + "' to '" + t->ToString() + "'!", *current_token);
}

Scope* CompilerContext::PushScope()
{
	auto temp = this->scope;
	this->scope = new Scope;
	this->scope->prev = temp;

	temp->next.push_back(this->scope);
	return this->scope;
}