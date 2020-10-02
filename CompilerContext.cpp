#include "Compiler.h"
#include "CompilerContext.h"
#include "Types/Function.h"

#include "Lexer.h"
#include "Expressions.h"
#include "DeclarationExpressions.h"

#ifdef _DEBUG
#ifndef DBG_NEW
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#define new DBG_NEW
#endif
#endif  // _DEBUG


using namespace Jet;

static unsigned int uuid = 5;

CompilerContext* CompilerContext::StartFunctionDefinition(Function* func)
{
	func->Load(this->root);

	auto n = new CompilerContext(this->root, this);
	n->function = func;
	func->context = n;
	llvm::BasicBlock *bb = llvm::BasicBlock::Create(root->context, "entry", n->function->f);
	root->builder.SetInsertPoint(bb);

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
	else if (value.type->IsInteger())//value.type->type == Types::Int || value.type->type == Types::Short || value.type->type == Types::Char)
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
			return CValue(value.type->base, this->root->builder.CreateLoad(value.val), value.val);
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

void CompilerContext::Store(CValue loc, CValue val, bool RVO)
{
	if (loc.type->base->type == Types::Struct && RVO == false)
	{
		if (loc.type->base->data->is_class == true)
			this->root->Error("Cannot copy class '" + loc.type->base->data->name + "' unless it has a copy operator.", *this->current_token);

		// Handle equality operator if we can find it
		auto funiter = val.type->data->functions.find("=");
		//todo: search through multimap to find one with the right number of args
		if (funiter != val.type->data->functions.end() && funiter->second->arguments.size() == 2)
		{
			llvm::Value* pointer = val.pointer;
			if (val.pointer == 0)//its a value type (return value)
			{
				//ok, lets be dumb
				//copy it to an alloca
				auto TheFunction = this->function->f;
				llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
					TheFunction->getEntryBlock().begin());
				pointer = TmpB.CreateAlloca(val.type->GetLLVMType(), 0, "return_pass_tmp");
				this->root->builder.CreateStore(val.val, pointer);
			}

			Function* fun = funiter->second;
			fun->Load(this->root);
			std::vector<CValue> argsv = { loc, CValue(loc.type, pointer) };

			fun->Call(this, argsv, true);

			if (pointer != val.pointer)
			{
				//destruct temp one
				this->Destruct(CValue(val.type->GetPointerType(), pointer), 0);
			}
			return;
		}
	}
	
	auto vall = this->DoCast(loc.type->base, val);

	// Handle copying
	if (loc.type->base->type == Types::Struct)
	{
		if (vall.val)
		{
			llvm::Instruction* I = llvm::dyn_cast<llvm::Instruction>(vall.val);
			I->eraseFromParent();// remove the load so we dont freak out llvm doing a struct copy
		}
		auto dptr = root->builder.CreatePointerCast(loc.val, root->CharPointerType->GetLLVMType());
		auto sptr = root->builder.CreatePointerCast(vall.pointer, root->CharPointerType->GetLLVMType());
		root->builder.CreateMemCpy(dptr, 0, sptr, 0, loc.type->base->GetSize());// todo properly handle alignment
		return;
	}
	else if (loc.type->base->type == Types::InternalArray)
	{
		llvm::Instruction* I = llvm::dyn_cast<llvm::Instruction>(vall.val);
		I->eraseFromParent();// remove the load so we dont freak out llvm doing a struct copy
		auto dptr = root->builder.CreatePointerCast(loc.val, root->CharPointerType->GetLLVMType());
		auto sptr = root->builder.CreatePointerCast(vall.pointer, root->CharPointerType->GetLLVMType());
		root->builder.CreateMemCpy(dptr, 0, sptr, 0, loc.type->base->GetSize());// todo properly handle alignment
		return;
	}
	
	root->builder.CreateStore(vall.val, loc.val);
}

CValue CompilerContext::BinaryOperation(Jet::TokenType op, CValue left, CValue lhsptr, CValue right)
{
	llvm::Value* res = 0;

	if (left.type->type == Types::Pointer && right.type->IsInteger())
	{
		//ok, lets just do a GeP
		if (op == TokenType::Plus)
			return CValue(left.type, this->root->builder.CreateGEP(left.val, right.val));
		else if (op == TokenType::Minus)
			return CValue(left.type, this->root->builder.CreateGEP(left.val, this->root->builder.CreateNeg(right.val)));
	}
	else if (left.type->type == Types::Struct)
	{
		//operator overloads
		const static std::map<Jet::TokenType, std::string> token_to_string = {
			{ TokenType::Plus, "+" },
			{ TokenType::Minus, "-" },
			{ TokenType::Slash, "/" },
			{ TokenType::Asterisk, "*" },
			{ TokenType::LeftShift, "<<" },
			{ TokenType::RightShift, ">>" },
			{ TokenType::BAnd, "&" },
			{ TokenType::BOr, "|" },
			{ TokenType::Xor, "^" },
			{ TokenType::Modulo, "%" },
			{ TokenType::LessThan, "<" },
			{ TokenType::GreaterThan, ">" },
			{ TokenType::LessThanEqual, "<=" },
			{ TokenType::GreaterThanEqual, ">=" }
		};
		auto res = token_to_string.find(op);
		if (res != token_to_string.end())
		{
			auto funiter = left.type->data->functions.find(res->second);
			//todo: search through multimap to find one with the right number of args
			//check args
			if (funiter != left.type->data->functions.end() && funiter->second->arguments.size() == 2)
			{
				Function* fun = funiter->second;
				fun->Load(this->root);
				std::vector<CValue> argsv = { lhsptr, right };
				return fun->Call(this, argsv, true);//for now lets keep these operators non-virtual
			}
		}
	}

	//try to do a cast
	right = this->DoCast(left.type, right);

	if (left.type->type != right.type->type)
	{
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
	else if (left.type->IsInteger())
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
			if (left.type->IsSignedInteger())//signed
				res = root->builder.CreateSDiv(left.val, right.val);
			else//unsigned
				res = root->builder.CreateUDiv(left.val, right.val);
			break;
		case TokenType::Modulo:
			if (left.type->IsSignedInteger())//signed
				res = root->builder.CreateSRem(left.val, right.val);
			else//unsigned
				res = root->builder.CreateURem(left.val, right.val);
			break;
		case TokenType::LessThan:
			//use U or S?
			if (left.type->IsSignedInteger())
				res = root->builder.CreateICmpSLT(left.val, right.val);
			else
				res = root->builder.CreateICmpULT(left.val, right.val);
			return CValue(root->BoolType, res);
			break;
		case TokenType::LessThanEqual:
			if (left.type->IsSignedInteger())
				res = root->builder.CreateICmpSLE(left.val, right.val);
			else
				res = root->builder.CreateICmpULE(left.val, right.val);
			return CValue(root->BoolType, res);
			break;
		case TokenType::GreaterThan:
			if (left.type->IsSignedInteger())
				res = root->builder.CreateICmpSGT(left.val, right.val);
			else
				res = root->builder.CreateICmpUGT(left.val, right.val);
			return CValue(root->BoolType, res);
			break;
		case TokenType::GreaterThanEqual:
			if (left.type->IsSignedInteger())
				res = root->builder.CreateICmpSGE(left.val, right.val);
			else
				res = root->builder.CreateICmpUGE(left.val, right.val);
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
			res = root->builder.CreateShl(left.val, right.val);
			break;
		case TokenType::RightShift:
			res = root->builder.CreateLShr(left.val, right.val);
			break;
		default:
			this->root->Error("Invalid Binary Operation '" + TokenToString[op] + "' On Type '" + left.type->ToString() + "'", *current_token);

			break;
		}

		return CValue(left.type, res);
	}
	else if (left.type->type == Types::Pointer)
	{
		//com
		switch (op)
		{
		case TokenType::Equals:
			res = root->builder.CreateICmpEQ(left.val, right.val);
			return CValue(root->BoolType, res);
			break;
		case TokenType::NotEqual:
			res = root->builder.CreateICmpNE(left.val, right.val);
			return CValue(root->BoolType, res);
			break;
		}
	}

	this->root->Error("Invalid Binary Operation '" + TokenToString[op] + "' On Type '" + left.type->ToString() + "'", *current_token);
}

Function* CompilerContext::GetMethod(const std::string& name, const std::vector<Type*>& args, Type* Struct)
{
	Function* fun = 0;

	if (Struct == 0)
	{
		//global function?
		auto iter = this->root->GetFunction(name, args);
		if (iter == 0)
		{
			//check if its a type, if so try and find a constructor
			auto type = this->root->TryLookupType(name);
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
					return fun;
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
				auto fun = type->data->functions.find(type->data->template_base->name);
				if (fun != type->data->functions.end())
					return fun->second;
				else
					return 0;
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
			for (unsigned int i = 0; i < fun->templates.size(); i++)
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
						//get the name of the variable
						unsigned int subl = 0;
						for (; subl < iii.first->name.length(); subl++)
						{
							if (!IsLetter(iii.first->name[subl]))//todo this might break with numbers in variable names
								break;
						}
						//check if it refers to same type
						std::string sub = iii.first->name.substr(0, subl);
						//check if it refers to this type
						if (sub/*iii.first->name*/ == ii.second)
						{
							//found it
							if (templates[i] != 0 && templates[i] != args[i2])
								this->root->Error("Could not infer template type", *this->current_token);

							//need to convert back to root type
							Type* top_type = args[i2];
							Type* cur_type = args[i2];
							//work backwards to get to the type
							int pos = iii.first->name.size() - 1;
							while (pos >= 0)
							{
								char c = iii.first->name[pos];
								if (c == '*' && cur_type->type == Types::Pointer)
									cur_type = cur_type->base;
								else if (!IsLetter(c))
									this->root->Error("Could not infer template type", *this->current_token);
								pos--;
							}
							templates[i] = cur_type;
						}
						i2++;
					}
					i++;
				}
			}

			for (unsigned int i = 0; i < fun->templates.size(); i++)
			{
				if (templates[i] == 0)
					this->root->Error("Could not infer template type", *this->current_token);
			}

			auto oldname = fun->expression->name.text;
			fun->expression->name.text += '<';
			for (unsigned int i = 0; i < fun->templates.size(); i++)
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

CValue CompilerContext::Call(const std::string& name, const std::vector<CValue>& args, Type* Struct, bool devirtualize)
{
	std::vector<Type*> arsgs;
	for (auto ii : args)
		arsgs.push_back(ii.type);

	auto old_tok = this->current_token;
	Function* fun = this->GetMethod(name, arsgs, Struct);
	this->current_token = old_tok;

	if (fun == 0 && Struct == 0)
	{
		//try and find something like a variable or constructor in the global namespace
		auto type = this->root->TryLookupType(name);
		if (type != 0 && type->type == Types::Struct)// If we found a type, call its constructor like a function
		{
			// Look for its constructor
			auto range = type->data->functions.equal_range(name);
			for (auto ii = range.first; ii != range.second; ii++)
			{
				if (ii->second->arguments.size() == args.size())
					fun = ii->second;
			}
			if (fun)
			{
				//ok, we allocate, call then 
				//allocate thing
				type->Load(this->root);

				auto TheFunction = this->function->f;
				llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
					TheFunction->getEntryBlock().begin());
				auto Alloca = TmpB.CreateAlloca(type->GetLLVMType(), 0, "constructortemp");

				std::vector<llvm::Value*> argsv;
				int i = 1;
				argsv.push_back(Alloca);// add 'this' ptr
				for (auto ii : args)// add other arguments
					argsv.push_back(this->DoCast(fun->arguments[i++].first, ii).val);//try and cast to the correct type if we can

				fun->Load(this->root);

				this->root->builder.CreateCall(fun->f, argsv);

				return CValue(type, this->root->builder.CreateLoad(Alloca), Alloca);
			}
			else // Fake a constructor if we couldnt find one
			{
				type->Load(this->root);

				if (type->type == Types::Struct)
				{
					auto TheFunction = this->function->f;
					llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
						TheFunction->getEntryBlock().begin());
					auto Alloca = TmpB.CreateAlloca(type->GetLLVMType(), 0, "constructortemp");

					//store defaults here...
					return CValue(type, this->root->builder.CreateLoad(Alloca), Alloca);
				}

				return CValue(type, type->GetDefaultValue(this->root));
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

					//get the template param to examine the type
					auto type = var.type->base->data->members.begin()->second.ty;// .find("T")->second.ty;

					if (args.size() != type->function->args.size())
						this->root->Error("Too many args in function call got " + std::to_string(args.size()) + " expected " + std::to_string(type->function->args.size()), *this->current_token);

					std::vector<llvm::Value*> argsv;
					for (unsigned int i = 0; i < args.size(); i++)
						argsv.push_back(this->DoCast(type->function->args[i], args[i]).val);//try and cast to the correct type if we can

					//add the data
					auto data_ptr = this->root->builder.CreateGEP(var.val, { this->root->builder.getInt32(0), this->root->builder.getInt32(1) });
					data_ptr = this->root->builder.CreateGEP(data_ptr, { this->root->builder.getInt32(0), this->root->builder.getInt32(0) });
					argsv.push_back(data_ptr);

					llvm::Value* fun = this->root->builder.CreateLoad(function_ptr);

					auto rtype = fun->getType()->getContainedType(0)->getContainedType(0);
					std::vector<llvm::Type*> fargs;
					for (unsigned int i = 1; i < fun->getType()->getContainedType(0)->getNumContainedTypes(); i++)
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
			for (unsigned int i = 0; i < args.size(); i++)
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

	//todo: fixme this isnt a very reliable fix
	if (args.size() + (fun->return_type->type == Types::Struct ? 1 : 0) != fun->f->arg_size() && fun->arguments.size() > 0 && fun->arguments[0].second == "this")
	{	
		//ok, we allocate, call then 
		//allocate thing
		auto type = fun->arguments[0].first->base;
		type->Load(this->root);

		auto TheFunction = this->function->f;
		llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
			TheFunction->getEntryBlock().begin());

		auto Alloca = TmpB.CreateAlloca(type->GetLLVMType(), 0, "constructortemp");

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
	else if (args.size() != fun->f->arg_size() && fun->return_type->type != Types::Struct)
	{
		this->root->Error("Function expected " + std::to_string(fun->f->arg_size()) + " arguments, got " + std::to_string(args.size()), *this->current_token);
	}

	std::vector<CValue> argsv;
	for (unsigned int i = 0; i < args.size(); i++)
		argsv.push_back(this->DoCast(fun->arguments[i].first, args[i]));//try and cast to the correct type if we can

	return fun->Call(this, argsv, devirtualize);
}

void CompilerContext::SetDebugLocation(const Token& t)
{
	assert(this->function->loaded);
	this->root->builder.SetCurrentDebugLocation(llvm::DebugLoc::get(t.line, t.column, this->function->scope));
}

CValue CompilerContext::GetVariable(const std::string& name, bool* is_const)
{
	auto cur = this->scope;
	CValue value;
	do
	{
		auto iter = cur->named_values.find(name);
		if (iter != cur->named_values.end())
		{
			value = iter->second.value;
			if (is_const)
			{
				*is_const = iter->second.is_const;
			}
			break;
		}
		cur = cur->prev;
	} while (cur);

	if (value.type->type == Types::Void)
	{
		if (is_const)
		{
			*is_const = false;
		}
		auto sym = this->root->GetVariableOrFunction(name);
		if (sym.type != SymbolType::Invalid)
		{
			if (sym.type == SymbolType::Function)
			{
				auto function = sym.fn;
				function->Load(this->root);
				return CValue(function->GetType(this->root), function->f);
			}
			else if (sym.type == SymbolType::Variable)
			{
				//variable
				return *sym.val;
			}
		}

		if (this->function->is_lambda)
		{
			auto var = this->parent->GetVariable(name);

			//look in locals above me
			CValue location = this->Load("_capture_data");
			auto storage_t = this->function->lambda.storage_type;

			//todo make sure this is the right location to do all of this
			//append the new type
			std::vector<llvm::Type*> types;
			for (unsigned int i = 0; i < this->captures.size(); i++)
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
	if (this->function == 0)
	{
		this->root->Error("Cannot return from outside function!", *current_token);
	}

	// Call destructors
	auto cur = this->scope;
	do
	{
		if (cur->destructed == false)
		{
			cur->Destruct(this, ret.pointer);
		}

		cur->destructed = true;
		cur = cur->prev;
	} while (cur);

	if (ret.type->type == Types::Void)
	{
		return root->builder.CreateRetVoid();
	}
	else if (ret.type->type == Types::Struct)
	{
		llvm::Instruction* I = llvm::dyn_cast<llvm::Instruction>(ret.val);
		I->eraseFromParent();// remove the load so we dont freak out llvm doing a struct copy

		//do a memcpy
		auto dptr = root->builder.CreatePointerCast(this->function->f->arg_begin(), this->root->CharPointerType->GetLLVMType());
		auto sptr = root->builder.CreatePointerCast(ret.pointer, this->root->CharPointerType->GetLLVMType());
		root->builder.CreateMemCpy(dptr, 0, sptr, 0, ret.type->GetSize());// todo properly handle alignment
		return root->builder.CreateRetVoid();
	}

	// Try and cast to the return type if we can
	if (ret.val)
	{
		ret = this->DoCast(this->function->return_type, ret);
	}
	return root->builder.CreateRet(ret.val);
}

CValue CompilerContext::DoCast(Type* t, CValue value, bool Explicit)
{
	if (value.type->type == t->type && value.type->data == t->data)
		return value;

	llvm::Type* tt = t->GetLLVMType();
	if (value.type->type == Types::Float && t->type == Types::Double)
	{
		return CValue(t, root->builder.CreateFPExt(value.val, tt));
	}
	if (value.type->type == Types::Double && t->type == Types::Float)
	{
		// Todo warn for downcasting if implicit
		return CValue(t, root->builder.CreateFPTrunc(value.val, tt));
	}
	if (value.type->type == Types::Double || value.type->type == Types::Float)
	{
		//float to int
		if (t->IsSignedInteger())
			return CValue(t, root->builder.CreateFPToSI(value.val, tt));
		else if (t->IsInteger())
			return CValue(t, root->builder.CreateFPToUI(value.val, tt));

		//todo: maybe do a warning if implicit from float->int or larger as it cant directly fit 1 to 1
	}
	if (value.type->IsInteger())
	{
		//int to float
		if (t->type == Types::Double || t->type == Types::Float)
		{
			if (value.type->IsSignedInteger())
			{
				return CValue(t, root->builder.CreateSIToFP(value.val, tt));
			}
			else
			{
				return CValue(t, root->builder.CreateUIToFP(value.val, tt));
			}
		}
		if (t->type == Types::Bool)
		{
			return CValue(t, root->builder.CreateIsNotNull(value.val));
		}
		if (t->type == Types::Pointer)
		{
			llvm::ConstantInt* ty = llvm::dyn_cast<llvm::ConstantInt>(value.val);
			if (Explicit == false && (ty == 0 || ty->getSExtValue() != 0))
			{
				root->Error("Cannot cast a non-zero integer value to pointer implicitly.", *this->current_token);
			}

			return CValue(t, root->builder.CreateIntToPtr(value.val, t->GetLLVMType()));
		}

		// This turned out to be hard....
		/*// Disallow implicit casts to smaller types
		if (!Explicit && t->GetSize() < value.type->GetSize())
		{
			// for now just ignore literals
			llvm::ConstantInt* is_literal = llvm::dyn_cast<llvm::ConstantInt>(value.val);
			if (!is_literal)
			{
				root->Error("Cannot implicitly cast from '" + value.type->ToString() + "' to a smaller integer type: '" + t->ToString() + "'.", *this->current_token);
			}
		}*/

		if (t->IsSignedInteger())
			return CValue(t, root->builder.CreateSExtOrTrunc(value.val, tt));
		else if (t->IsInteger())
			return CValue(t, root->builder.CreateZExtOrTrunc(value.val, tt));
	}
	if (value.type->type == Types::Pointer)
	{
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
				return CValue(t, root->builder.CreatePointerCast(value.val, t->GetLLVMType(), "ptr2ptr"));
			}
			else if (t->IsInteger())
			{
				return CValue(t, root->builder.CreatePtrToInt(value.val, t->GetLLVMType(), "ptr2int"));
			}
		}
	}
	if (value.type->type == Types::InternalArray)
	{
		if (t->type == Types::Array && t->base == value.type->base)
		{
			// construct an array and use that 
			auto str_type = root->GetArrayType(value.type->base);

			//alloc the struct for it
			auto Alloca = root->builder.CreateAlloca(str_type->GetLLVMType(), root->builder.getInt32(1), "into_array");

			auto size = root->builder.getInt32(value.type->size);

			//store size
			auto size_p = root->builder.CreateGEP(Alloca, { root->builder.getInt32(0), root->builder.getInt32(0) });
			root->builder.CreateStore(size, size_p);

			//store data pointer
			auto data_p = root->builder.CreateGEP(Alloca, { root->builder.getInt32(0), root->builder.getInt32(1) });

			auto data_v = root->builder.CreateGEP(value.pointer, { root->builder.getInt32(0), root->builder.getInt32(0) });
			//value.val->dump();
			//value.val->getType()->dump();
			//value.pointer->dump();
			//value.pointer->getType()->dump();
			root->builder.CreateStore(data_v, data_p);

			// todo do we need this load?
			return CValue(t, this->root->builder.CreateLoad(Alloca));
		}
	}
	if (value.type->type == Types::Array)
	{
		if (t->type == Types::Pointer)// array to pointer
		{
			if (t->base == value.type->base)
			{
				//ok for now lets not allow this, users can just access the ptr field
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
		for (unsigned int i = 0; i < t->_union->members.size(); i++)
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

//reduce redundancy by making one general version that just omits the llvm::Value from the CValue when doign a check
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
		if (t->IsInteger())//t->type == Types::Int || t->type == Types::Short || t->type == Types::Char)
			return true;// CValue(t, root->builder.CreateFPToSI(value.val, tt));

		//remove me later float to bool
		//if (t->type == Types::Bool)
		//return CValue(t, root->builder.CreateFCmpONE(value.val, llvm::ConstantFP::get(llvm::getGlobalContext(), llvm::APFloat(0.0))));
	}
	if (value.type->IsInteger())//value.type->type == Types::Int || value.type->type == Types::Short || value.type->type == Types::Char)
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
		if (t->IsInteger())//t->type == Types::Int || t->type == Types::Short || t->type == Types::Char)
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
			else if (t->IsInteger())
				return true;
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
		for (unsigned int i = 0; i < t->_union->members.size(); i++)
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
		for (unsigned int i = 0; i < traits.size(); i++)
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

void Scope::Destruct(CompilerContext* context, llvm::Value* ignore)
{
	for (auto& ii : this->named_values)
	{
		auto& value = ii.second.value;
		if (value.val == ignore)
			continue;//dont destruct what we are returning
		if (value.type->type == Types::Pointer && value.type->base->type == Types::Struct)
			context->Destruct(value, 0);
		//else if (ii.second.type->type == Types::Pointer && ii.second.type->base->type == Types::Array && ii.second.type->base->base->type == Types::Struct)
		//	context->Destruct(CValue(ii.second.type->base, ii.second.val), context->root->builder.getInt32(ii.second.type->base->size));
	}

	for (auto ii : this->to_destruct)
	{
		if (ii.type->type == Types::Pointer && ii.type->base->type == Types::Array && ii.type->base->base->type == Types::Struct)
		{
			auto loc = context->root->builder.CreateGEP(ii.val, { context->root->builder.getInt32(0), context->root->builder.getInt32(0) });
			auto size = context->root->builder.CreateLoad(loc);

			auto ptr = context->root->builder.CreateGEP(ii.val, { context->root->builder.getInt32(0), context->root->builder.getInt32(1) });
			ptr = context->root->builder.CreateLoad(ptr);
			context->Destruct(CValue(ii.type->base->GetPointerType() , ptr), size);
		}
	}
	this->destructed = true;
}

void CompilerContext::PopScope()
{
	//call destructors
	if (this->scope->prev != 0)
	{
		if (this->scope->destructed == false)
			this->scope->Destruct(this);

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

		int size = 0;
		for (unsigned int i = 0; i < this->captures.size(); i++)
		{
			auto var = this->captures[i];

			//get pointer to data location
			auto ptr = root->builder.CreateGEP(lambda, { root->builder.getInt32(0), root->builder.getInt32(1) }, "lambda_data");
			ptr = root->builder.CreatePointerCast(ptr, storage_t->getPointerTo());
			ptr = root->builder.CreateGEP(ptr, { root->builder.getInt32(0), root->builder.getInt32(i) });

			//then store it
			auto val = parent->Load(var);
			size += val.type->GetSize();
			root->builder.CreateStore(val.val, ptr);
		}
		if (size > 64)
		{
			this->root->Error("Capture size too big! Captured " + std::to_string(size) + " bytes but max was 64!", *this->current_token);
		}
	}
	this->captures.clear();
}

CValue CompilerContext::Load(const std::string& name)
{
	CValue value = GetVariable(name);
	if (value.type->type == Types::Function || value.type->type == Types::Int)// last case is for constants
		return value;

	return CValue(value.type->base, root->builder.CreateLoad(value.val, name.c_str()), value.val);
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
		if (fun == 0)
			return;//todo: is this right behavior?
		fun->Load(this->root);
		if (arr_size == 0)
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
			fun = ty->GetMethod("~" + ty->data->template_base->name, { pointer.type }, this);
		else
			fun = ty->GetMethod("~" + ty->data->name, { pointer.type }, this);
		if (fun == 0)
			return;
		fun->Load(this->root);
		if (arr_size == 0)
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