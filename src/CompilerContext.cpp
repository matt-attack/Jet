#include "Compiler.h"
#include "CompilerContext.h"
#include "types/Function.h"

#include "Lexer.h"
#include "expressions/Expressions.h"
#include "expressions/DeclarationExpressions.h"

#ifdef _DEBUG
#ifndef DBG_NEW
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#define new DBG_NEW
#endif
#endif  // _DEBUG


using namespace Jet;

CompilerContext* CompilerContext::StartFunctionDefinition(Function* func)
{
	auto n = new CompilerContext(this->root, this);
	n->function = func;
	func->context_ = n;

	func->Load(this->root);

	llvm::BasicBlock *bb = llvm::BasicBlock::Create(root->context, "entry", n->function->f_);
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

    if (!value.val)
    {
        value.val = root->builder.CreateLoad(value.pointer);
    }

	if (value.type->type == Types::Float || value.type->type == Types::Double)
	{
		switch (operation)
		{
		case TokenType::Minus:
			res = root->builder.CreateFNeg(value.val);
			break;
		default:
			this->root->Error("Invalid Unary Operation '" + TokenToString[operation] + "' On Type '" + value.type->ToString() + "'", current_token);
			break;
		}

		return CValue(value.type, res);
	}
	else if (value.type->IsInteger())
	{
		const auto& type = value.type->type;
		switch (operation)
		{
		case TokenType::Increment:
			if (type == Types::Int || type == Types::UInt)
				res = root->builder.CreateAdd(value.val, root->builder.getInt32(1));
			else if (type == Types::Short || type == Types::UShort)
				res = root->builder.CreateAdd(value.val, root->builder.getInt16(1));
			else if (type == Types::Char || type == Types::UChar)
				res = root->builder.CreateAdd(value.val, root->builder.getInt8(1));
			else if (type == Types::Long || type == Types::ULong)
				res = root->builder.CreateAdd(value.val, root->builder.getInt64(1));
			break;
		case TokenType::Decrement:
			if (type == Types::Int || type == Types::UInt)
				res = root->builder.CreateSub(value.val, root->builder.getInt32(1));
			else if (type == Types::Short || type == Types::UShort)
				res = root->builder.CreateSub(value.val, root->builder.getInt16(1));
			else if (type == Types::Char || type == Types::UChar)
				res = root->builder.CreateSub(value.val, root->builder.getInt8(1));
			else if (type == Types::Long || type == Types::ULong)
				res = root->builder.CreateSub(value.val, root->builder.getInt64(1));
			break;
		case TokenType::Minus:
			res = root->builder.CreateNeg(value.val);
			break;
		case TokenType::BNot:
			res = root->builder.CreateNot(value.val);
			break;
		default:
			this->root->Error("Invalid Unary Operation '" + TokenToString[operation] + "' On Type '" + value.type->ToString() + "'", current_token);
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
			this->root->Error("Invalid Unary Operation '" + TokenToString[operation] + "' On Type '" + value.type->base->ToString() + "'", current_token);
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
	this->root->Error("Invalid Unary Operation '" + TokenToString[operation] + "' On Type '" + value.type->ToString() + "'", current_token);
}

void CompilerContext::Store(CValue loc, CValue in_val, bool RVO)
{	
	auto val = this->DoCast(loc.type->base, in_val);

	if (loc.type->base->type == Types::Struct && RVO == false && val.type->type == Types::Struct && loc.type->base == val.type)
	{
		if (loc.type->base->data->is_class == true)
			this->root->Error("Cannot copy class '" + loc.type->base->data->name + "' unless it has a copy operator.", this->current_token);

        auto stru = val.type->data;
		// Handle equality operator if we can find it
		auto fun_iter = stru->functions.find("=");
		//todo: search through multimap to find one with the right number of args
		if (fun_iter != stru->functions.end() && fun_iter->second->arguments_.size() == 2)
		{
			llvm::Value* pointer = val.pointer;
			if (val.pointer == 0)//its a value type (return value)
			{
				//ok, lets be dumb
				//copy it to an alloca
				auto TheFunction = this->function->f_;
				llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
					TheFunction->getEntryBlock().begin());
				pointer = TmpB.CreateAlloca(val.type->GetLLVMType(), 0, "return_pass_tmp");
				this->root->builder.CreateStore(val.val, pointer);
			}

			Function* fun = fun_iter->second;
			fun->Load(this->root);
			std::vector<FunctionArgument> argsv = { {loc, 0}, {val, 0} };

			fun->Call(this, argsv, true);

			if (pointer != val.pointer)
			{
				//destruct temp one
				this->Destruct(CValue(val.type, 0, pointer), 0);
			}
			return;
		}
	}

	// Handle copying
	if (loc.type->base->type == Types::Struct)
	{
		if (val.val)
		{
			llvm::Instruction* I = llvm::dyn_cast<llvm::Instruction>(val.val);
			I->eraseFromParent();// remove the load so we dont freak out llvm doing a struct copy
		}
		auto dptr = root->builder.CreatePointerCast(loc.val, root->CharPointerType->GetLLVMType());
		auto sptr = root->builder.CreatePointerCast(val.pointer, root->CharPointerType->GetLLVMType());
		root->builder.CreateMemCpy(dptr, 0, sptr, 0, loc.type->base->GetSize());// todo properly handle alignment
		return;
	}
	else if (loc.type->base->type == Types::InternalArray)
	{
        if (val.val)
        {
		    llvm::Instruction* I = llvm::dyn_cast<llvm::Instruction>(val.val);
		    I->eraseFromParent();// remove the load so we dont freak out llvm doing a struct copy
        }
		auto dptr = root->builder.CreatePointerCast(loc.val, root->CharPointerType->GetLLVMType());
		auto sptr = root->builder.CreatePointerCast(val.pointer, root->CharPointerType->GetLLVMType());
		root->builder.CreateMemCpy(dptr, 0, sptr, 0, loc.type->base->GetSize());// todo properly handle alignment
		return;
	}

    if (!val.val)
    {
        val.val = root->builder.CreateLoad(val.pointer);
    }
	
	root->builder.CreateStore(val.val, loc.val);
}

CValue CompilerContext::BinaryOperation(Jet::TokenType op, CValue left, CValue right)
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
			{ TokenType::GreaterThanEqual, ">=" },
			{ TokenType::Equals, "==" }
		};
        if (!left.pointer)
        {
            this->root->Error("Unexpected error", Token());
        }
        //CValue lhsptr(left.type->GetPointerType(), left.pointer);
        // special case for != (call == and then not it)
        if (op == TokenType::NotEqual)
        {
			auto funiter = left.type->data->functions.find("==");
			if (funiter != left.type->data->functions.end() && funiter->second->arguments_.size() == 2)
			{
				Function* fun = funiter->second;
				fun->Load(this->root);
				std::vector<FunctionArgument> argsv = { {left, 0}, {right, 0} };// todo probably need to enforce the return type
                CValue ret = fun->Call(this, argsv, true);
                ret.val = root->builder.CreateNot(ret.val);
				return ret;
			}
        }
		auto res = token_to_string.find(op);
		if (res != token_to_string.end())
		{
			auto funiter = left.type->data->functions.find(res->second);
			if (funiter != left.type->data->functions.end() && funiter->second->arguments_.size() == 2)
			{
				Function* fun = funiter->second;
				fun->Load(this->root);
				std::vector<FunctionArgument> argsv = { {left, 0}, {right, 0} };
				return fun->Call(this, argsv, true);//for now lets keep these operators non-virtual
			}
		}
	}

	//try to do a cast
	right = this->DoCast(left.type, right);

    if (!right.val)
    {
        right.val = root->builder.CreateLoad(right.pointer);
    }

    if (!left.val)
    {
        left.val = root->builder.CreateLoad(left.pointer);
    }

	if (left.type->type != right.type->type)
	{
		root->Error("Cannot perform a binary operation between two incompatible types", this->current_token);
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
			this->root->Error("Invalid Binary Operation '" + TokenToString[op] + "' On Type '" + left.type->ToString() + "'", current_token);

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
		case TokenType::ShlAssign:
		case TokenType::LeftShift:
			res = root->builder.CreateShl(left.val, right.val);
			break;
		case TokenType::ShrAssign:
		case TokenType::RightShift:
			res = root->builder.CreateLShr(left.val, right.val);
			break;
		default:
			this->root->Error("Invalid Binary Operation '" + TokenToString[op] + "' On Type '" + left.type->ToString() + "'", current_token);

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
    else if (left.type->type == Types::Bool)
    {
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

	this->root->Error("Invalid Binary Operation '" + TokenToString[op] + "' On Type '" + left.type->ToString() + "'", current_token);
}

Symbol CompilerContext::GetMethod(
  const std::string& name,
  const std::vector<Type*>& args,
  Type* Struct,
  bool& is_constructor)
{
	Function* fun = 0;
    is_constructor = false;
	if (Struct == 0)
	{
        CValue var = this->GetVariable(name, false, false);

        if (var.val || var.pointer)
        {
            return new CValue(var);
        }

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
				auto iter = type->data->functions.find("new");
				if (iter != type->data->functions.end())
				{
                    is_constructor = true;
					return iter->second;
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
					return Symbol();
			}
			//instantiate here
			this->root->Error("Not implemented", this->current_token);

			//return range.first->second;
		}

		//look for the best one
		fun = this->root->GetFunction(name, args);

		if (fun && fun->templates_.size() > 0)
		{
			auto templates = new Type*[fun->templates_.size()];
			for (unsigned int i = 0; i < fun->templates_.size(); i++)
				templates[i] = 0;

			//need to infer
			if (fun->arguments_.size() > 0)
			{
				int i = 0;
				for (auto ii : fun->templates_)
				{
					//look for stuff in args
					int i2 = 0;
					for (auto iii : fun->arguments_)
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
								this->root->Error("Could not infer template type", this->current_token);

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
									this->root->Error("Could not infer template type", this->current_token);
								pos--;
							}
							templates[i] = cur_type;
						}
						i2++;
					}
					i++;
				}
			}

			for (unsigned int i = 0; i < fun->templates_.size(); i++)
			{
				if (templates[i] == 0)
					this->root->Error("Could not infer template type", this->current_token);
			}

            auto& fun_name = fun->expression_->data_.signature.name.text;

			const auto oldname = fun_name;
			fun_name += '<';
			for (unsigned int i = 0; i < fun->templates_.size(); i++)
			{
				fun_name += templates[i]->ToString();
				if (i + 1 < fun->templates_.size())
					fun_name += ',';
			}
			fun_name += '>';
			auto rname = fun_name;

			//register the types
			int i = 0;
			for (auto ii : fun->templates_)
			{
				//check if traits match
				if (templates[i]->MatchesTrait(this->root, ii.first->trait) == false)
					root->Error("Type '" + templates[i]->name + "' doesn't match Trait '" + ii.first->name + "'", root->current_function->current_token);

				root->ns->members.insert({ ii.second, templates[i++] });
			}

			//store then restore insertion point
			auto rp = root->builder.GetInsertBlock();
			auto dp = root->builder.getCurrentDebugLocation();


			fun->expression_->CompileDeclarations(this);
			fun->expression_->DoCompile(this);

			root->builder.SetCurrentDebugLocation(dp);
			if (rp)
				root->builder.SetInsertPoint(rp);

			fun_name = oldname;

			//time to recompile and stuff
			return root->ns->members.find(rname)->second.fn;
		}
		return fun;
	}
	else
	{
        // todo look for members
		return Struct->GetMethod(name, args, this);
	}
}

#undef alloca

int minimum(int x, int y, int z)
{
    return std::min(x, std::min(y, z));
}

int Edit_Distance(const std::string& s1, const std::string& s2, int n, int m)
{
/* Here n = len(s1)
       m = len(s2) */
 
  if(n == 0 && m == 0)   //Base case
     return 0;
  if(n == 0)            //Base case
     return m;
  if( m == 0 )         //Base Case
     return n;
 
/* Recursive Part */
int   a  = Edit_Distance(s1, s2, n-1, m-1) + (s1[n-1] != s2[m-1]);
int   b  = Edit_Distance(s1, s2, n-1, m) + 1;                      //Deletion
int   c  = Edit_Distance(s1, s2, n, m-1) + 1;                      //Insertion
 
   return  minimum(a, b, c);
}

int levenshtein_distance(const std::string& name, const std::string& name2)
{
    return Edit_Distance(name, name2, name.length(), name2.length());
    /*if (name.length() == 0)
    {
        return name2.length();
    }
    else if (name2.length() == 0)
    {
        return name.length();
    }
    else if (name.length() == name2.length())
    {
        return levenshtein_distance(name.substr(1), name2.substr(1));
    }
    else
    {
        return minimum(levenshtein_distance(name.substr(1), name2) + 1,
                       levenshtein_distance(name, name2.substr(1)) + 1,
                       levenshtein_distance(name.substr(1), name2.substr(1))
                         + ((name[0] != name2[0]) ? 1 : 0));
    }*/
}

CValue CompilerContext::Call(const Token& name, const std::vector<FunctionArgument>& args, Type* Struct, bool devirtualize, bool is_const)
{
	std::vector<Type*> arsgs;
	for (auto ii : args)
	{
		arsgs.push_back(ii.value.type);
	}

	auto old_tok = this->current_token;
    bool is_constructor = false;
	Symbol function_symbol = this->GetMethod(name.text, arsgs, Struct, is_constructor);
	this->current_token = old_tok;

    if (is_constructor)
    {
        Function* fun = function_symbol.fn;// these must be functions

        // For constructors, we alloca the struct and then call it
        auto type = fun->arguments_[0].first->base;
		type->Load(this->root);

		auto TheFunction = this->function->f_;
		llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
			TheFunction->getEntryBlock().begin());
		auto Alloca = TmpB.CreateAlloca(type->GetLLVMType(), 0, "constructortemp");

		std::vector<FunctionArgument> argsv;
		argsv.push_back({CValue(type->GetPointerType(), Alloca), 0});// add 'this' ptr
		for (auto ii : args)// add other arguments
			argsv.push_back(ii);

        fun->Call(this, argsv, false);

		return CValue(type, 0, Alloca);
    }

    bool skip_first = false;// todo clean this up
	if (!function_symbol && Struct == 0)
	{
        // try and find a function
        std::string closest = GetSimilarMethod(name.text);
        if (closest.length())
        {
		    this->root->Error("Could not find function '" + BOLD(name.text) + "'. Did you mean '" + BOLD(closest) + "'?", name);
        }
        else
        {
		    this->root->Error("Could not find function '" + BOLD(name.text) + "'", name);
        }
	}
	else if (!function_symbol && Struct)
	{
        // Check for member variables which are callable
        int i = 0;
        for (auto& mem: Struct->data->struct_members)
        {
            if (mem.name == name.text)
            {
                CValue _this = args[0].value;
                /*if (!_this.val)
                {
                    _this.val = root->builder.CreateLoad(_this.pointer);
                }*/

	            std::vector<llvm::Value*> iindex = { root->builder.getInt32(0), root->builder.getInt32(i) };

	            auto loc = root->builder.CreateGEP(_this.pointer, iindex, "index");

                // load it m8
                CValue* val = new CValue(mem.type, 0, loc);
                function_symbol = val;
                skip_first = true;
            }
            i++;
        }
        if (!function_symbol)
        {
            // try and find a similar name
            std::string best; int min_dist = 5;
            for (auto& mem: Struct->data->struct_members)
            {
                int dist = levenshtein_distance(mem.name, name.text);
                if (dist < min_dist)
                {
                    best = mem.name;
                    min_dist = dist;
                }
            }
            for (auto& mem: Struct->data->functions)
            {
                int dist = levenshtein_distance(mem.first, name.text);
                if (dist < min_dist)
                {
                    best = mem.first;
                    min_dist = dist;
                }
            }
            if (best.length() == 0)
            {
		        this->root->Error("Function '" + BOLD(name.text) + "' is not defined on object '" + BOLD(Struct->ToString()) + "'", name);
            }
            else
            {
		        this->root->Error("Function '" + BOLD(name.text) + "' is not defined on object '" + BOLD(Struct->ToString()) + "'. Did you mean '" + BOLD(best) + "'?", name);
            }

            // try and find 
        }
	}

    // check for functors
    if (function_symbol.type != SymbolType::Function && function_symbol.val->type->type == Types::Struct)
    {
        CValue var = *function_symbol.val;
        
        auto funiter = var.type->data->functions.find("()"); 
        if (funiter != var.type->data->functions.end())
        {
            function_symbol.type = SymbolType::Function;
            delete function_symbol.val;// we had to allocate it, todo fix this weirdness
            function_symbol.fn = funiter->second;
        }
    }

    // handle actual function vs function pointer
    if (function_symbol.type == SymbolType::Function)
    {
        Function* fun = function_symbol.fn;
        if (!fun->is_const_ && is_const)
        {
            this->root->Error("Cannot call non-const function '" + BOLD(name.text) + "' on const value", this->current_token);
        }

	    return fun->Call(this, args, devirtualize);
    }
    else
    {
        // its a variable
        CValue var = *function_symbol.val;
        delete function_symbol.val;// we had to allocate it, todo fix this weirdness

        // verify that we can actually call it
        llvm::Value* append = 0;
        if (var.type->type != Types::Function)
        {
            // handle lambdas (quite hackily). todo fix this....
            if (var.type->type == Types::Struct &&
                var.type->data->template_base &&
                var.type->data->template_base->name == "function")
            {
                // To call a lambda, we need to cast the function pointer to add the capture data as
                // as last argument.
		        auto ptr = root->builder.CreateGEP(var.pointer, { root->builder.getInt32(0), root->builder.getInt32(0) }, "get_fn_ptr");
		        auto data_ptr = root->builder.CreateGEP(var.pointer, { root->builder.getInt32(0), root->builder.getInt32(1) }, "get_data_ptr");
                data_ptr = root->builder.CreatePointerCast(data_ptr, root->CharPointerType->GetLLVMType());

                auto old_fn_type = var.type->data->struct_members[0].type;

                auto ret = old_fn_type->function->return_type->GetLLVMType();

                std::vector<llvm::Type*> types;
                std::vector<Type*> types2;
                for (const auto& arg: old_fn_type->function->args)
                {
                    types.push_back(arg->GetLLVMType());
                    types2.push_back(arg);
                }
                types.push_back(root->CharPointerType->GetLLVMType());
                auto type = llvm::FunctionType::get(ret, types, false)->getPointerTo()->getPointerTo();
                var.val = 0;
		        var.pointer = root->builder.CreatePointerCast(ptr, type);

                var.type = root->GetFunctionType(old_fn_type->function->return_type, types2);
                //skip_first = false;
                append = data_ptr;
            }
            else
            {
                // functors have already been handled above so anything here is an error
                this->root->Error("Cannot call non-function type", this->current_token);
            }
        }

        std::vector<FunctionArgument> argsc;
        int skip = skip_first ? 1 : 0;// if skip first, we dont pass in struct as the first argument
		for (unsigned int i = skip; i < args.size(); i++)
        {
		    argsc.push_back(args[i]);//try and cast to the correct type if we can
        }
        if (append) argsc.push_back({CValue(this->root->CharPointerType, append), 0 });

        if (!var.val)
        {
            var.val = this->root->builder.CreateLoad(var.pointer);
        }

        return var.type->function->Call(this, var.val, argsc, false, 0, append);
    }
}

void CompilerContext::SetDebugLocation(const Token& t)
{
	assert(this->function->loaded_);
	this->root->builder.SetCurrentDebugLocation(llvm::DebugLoc::get(t.line, t.column, this->function->scope_));
}

std::string CompilerContext::GetSimilarMethod(const std::string& name)
{
    std::string closest; int min_distance = 8;

	auto cur = this->scope;
	CValue value(0, 0);
	do
	{
        for (const auto& val: cur->named_values)
        {
            int dist = levenshtein_distance(val.first, name);
            if (dist < min_distance)
            {
                min_distance = dist;
                closest = val.first;
            }
        }
		cur = cur->prev;
	} while (cur);

    bool namespaced = name.find_first_of(':') != std::string::npos;
    if (namespaced)
    {
        // First find the top level namespace indicated by this
        Namespace* new_ns = 0;
		Namespace* cur_ns = root->ns;

        int cur_pos = 0;

        bool first = true;
        do
        {
            int len = 0;
	        while (cur_pos+len < name.length() &&
                   (IsLetter(name[cur_pos+len]) || IsNumber(name[cur_pos + len])))
	        {
		        len++;
	        }
            if (cur_pos+len >= name.length()-1)
            {
                break;// stop before we hit the last bit
            }

            std::string ns = name.substr(cur_pos, len);

		    // Try and find this first namespace by recursively going up
            // If this is not the top level, only check the current namespace
            new_ns = 0;
		    do
		    {
			    auto res = cur_ns->members.find(ns);
			    if (res != cur_ns->members.end())
			    {
                    // handle structs
                    if (res->second.type == SymbolType::Type)
                    {
                        Type* t = res->second.ty;
                        if (t->type == Types::Struct)
                        {
                            new_ns = t->data;
                        }
                        break;
                    }

			    	new_ns = res->second.ns;
			    	break;
			    }
			    cur_ns = cur_ns->parent;
		    }
		    while (cur_ns && first);

            first = false;

            cur_ns = new_ns;
            cur_pos += len + 2;
        }
        while (true);

        if (!new_ns) { return closest; }

        for (const auto& member: new_ns->members)
        {
            if (member.second.type != SymbolType::Function &&
                member.second.type != SymbolType::Variable)
            {
                continue;
            }

            int dist = levenshtein_distance(member.first, name);
            if (dist < min_distance)
            {
                min_distance = dist;
                closest = member.first;
            }
        }
    }
    else
    {
	    //search up the namespace tree for this variable or function
	    auto next = root->ns;
	    do
	    {
            for (const auto& member: next->members)
            {
                if (member.second.type != SymbolType::Function &&
                    member.second.type != SymbolType::Variable)
                {
                    continue;
                }

                int dist = levenshtein_distance(member.first, name);
                if (dist < min_distance)
                {
                    min_distance = dist;
                    closest = member.first;
                }
            }

	    	next = next->parent;
    	}
        while (next && !namespaced);
    }

    return closest;
}

CValue CompilerContext::GetVariable(const std::string& name, bool error, bool include_functions)
{
	auto cur = this->scope;
	CValue value(0, 0);
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

	if (value.type == 0)
	{
		auto sym = this->root->GetVariableOrFunction(name);
		if (sym.type != SymbolType::Invalid)
		{
			if (sym.type == SymbolType::Function && include_functions)
			{
				auto function = sym.fn;
				function->Load(this->root);
				return CValue(function->GetType(this->root), function->f_, 0, true);
			}
			else if (sym.type == SymbolType::Variable)
			{
				//variable
				return *sym.val;
			}
		}

		if (this->function->is_lambda_)
		{
			auto var = this->parent->GetVariable(name);

			//look in locals above me
			CValue location = this->Load("_capture_data");
			auto storage_t = this->function->lambda_.storage_type;

			//todo make sure this is the right location to do all of this
			//append the new type
			std::vector<llvm::Type*> types;
			for (unsigned int i = 0; i < this->captures.size(); i++)
				types.push_back(storage_t->getContainedType(i));

			types.push_back(var.type->GetLLVMType());
			storage_t = this->function->lambda_.storage_type = storage_t->create(types);

			auto data = root->builder.CreatePointerCast(location.val, storage_t->getPointerTo());

			//load it, then store it as a local
			auto val = root->builder.CreateGEP(data, { root->builder.getInt32(0), root->builder.getInt32(this->captures.size()) });

			CValue value(var.type, 0, root->builder.CreateAlloca(var.type->GetLLVMType()));

			this->RegisterLocal(name, value, false, true);//need to register it as immutable 

			root->builder.CreateStore(root->builder.CreateLoad(val), value.pointer);
			this->captures.push_back(name);

			return value;
		}

        if (error)
        {
		    this->root->Error("Undeclared identifier '" + name + "'", current_token);
        }
        else
        {
            return CValue(root->VoidType, 0);
        }
	}
	return value;
}

llvm::ReturnInst* CompilerContext::Return(CValue ret)
{
	if (this->function == 0)
	{
		this->root->Error("Cannot return from outside function!", current_token);
	}

    auto return_type = this->function->return_type_;

    // If we are doing a struct return, first copy into the return value before destructing
    if (return_type->type == Types::Struct)
    {
        // do the cast!
        CValue cast = DoCast(return_type, ret, false, this->function->f_->arg_begin());

        if (cast.val)
        {
		  llvm::Instruction* I = llvm::dyn_cast<llvm::Instruction>(cast.val);
		  I->eraseFromParent();// remove the load so we dont freak out llvm doing a struct copy
        }


        // okay, try and do a copy
        auto copy_iter = cast.type->data->functions.end();// todo actually do and multimap
        auto assign_iter = cast.type->data->functions.find("=");// todo multimap
        // if a cast was done, we've already constructed
        if (cast.type != ret.type)
        {

        }
        //if we have a copy constructor , just call it
        else if (copy_iter != return_type->data->functions.end())// if we have copy constructor (todo)
        {

        }
        // todo wrap this into function?
        else if (assign_iter != return_type->data->functions.end())// if we have assignment operator
        {
            // if we have a constructor, call it
            this->Construct(CValue(return_type->GetPointerType(), this->function->f_->arg_begin()), 0);
            // next, call assign
			Function* fun = assign_iter->second;
			fun->Load(this->root);
			std::vector<FunctionArgument> argsv = { {CValue(return_type->GetPointerType(), this->function->f_->arg_begin()), 0}, {cast, 0} };

			fun->Call(this, argsv, true);
        }
        else // otherwise construct and then memcopy
        {
            // if we have a constructor, call it
            this->Construct(CValue(return_type->GetPointerType(), this->function->f_->arg_begin()), 0);

		    //do a memcpy
		    auto dptr = root->builder.CreatePointerCast(this->function->f_->arg_begin(), this->root->CharPointerType->GetLLVMType());
		    auto sptr = root->builder.CreatePointerCast(cast.pointer, this->root->CharPointerType->GetLLVMType());
		    root->builder.CreateMemCpy(dptr, 0, sptr, 0, return_type->GetSize());// todo properly handle alignment
        }
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

	if (return_type->type == Types::Void)
	{
		// Only allow this if the function return type is void
		if (this->function->return_type_->type != Types::Void)
		{
			this->root->Error("Cannot return void in function returning '"
								+ this->function->return_type_->ToString() + "'!", current_token);
		}
		return root->builder.CreateRetVoid();
	}
	else if (return_type->type == Types::Struct)
	{
		return root->builder.CreateRetVoid();
	}

	// Try and cast to the return type if we can
	ret = this->DoCast(return_type, ret);

    if (!ret.val)
    {
        ret.val = root->builder.CreateLoad(ret.pointer);
    }

	return root->builder.CreateRet(ret.val);
}

CValue CompilerContext::DoCast(Type* t, CValue value, bool Explicit, llvm::Value* alloca)
{
	if (value.type->type == t->type && value.type->data == t->data)
		return value;

    if (!value.val)
    {
        value.val = root->builder.CreateLoad(value.pointer);
    }

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
				root->Error("Cannot cast a non-zero integer value to pointer implicitly", this->current_token);
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
	if (t->type == Types::Union)
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
    if (t->type == Types::Struct)
    {
        // Look for a relevant constructor
        // for now find the exact match
        for (auto& f: t->data->functions)
        {
            if (f.first != t->data->name)
            {
                continue;
            }

            if (f.second->arguments_.size() != 2)
            {
                continue;
            }

            if (f.second->arguments_[1].first != value.type)
            {
                continue;
            }

            f.second->Load(this->root);

            // Okay, create the object, then construct it
            // first alloca and store it as the pointer
            CValue aalloca(t, 0, alloca ? alloca : this->root->builder.CreateAlloca(t->GetLLVMType(), 0, "cast"), false);
            // then call the constructor
            std::vector<FunctionArgument> v;
            v.push_back({aalloca, 0});
            v.push_back({value, 0});
            f.second->Call(this, v, false, true);

            if (!alloca)
            {
                DestructLater(aalloca);
            }

            //printf("found constructor for type %s\n", f.second->arguments[1].first->name.c_str());
            return aalloca;
        }
    }
    
    if (value.type->type == Types::InitializerList)
    {
    	// okay, an initializer list can cast to arrays, so lets do it
    	if (t->type == Types::Array)
    	{
    		// first, get base type and cast everything into it then stuff them into an array
    		Type* base_type = t->base;
    		
    		const std::vector<CValue>* list_elements = (std::vector<CValue>*)value.val;
    		
			// allocate the backing data
			auto ArrayAlloc = root->builder.CreateAlloca(t->base->GetLLVMType(), root->builder.getInt32(list_elements->size()), "init_list_to_arr_data");
			
			// now cast each element and stuff them in
			for (int i = 0; i < list_elements->size(); i++)
			{
				auto loc = root->builder.CreateGEP(ArrayAlloc, root->builder.getInt32(i));
				
				CValue casted = DoCast(base_type, (*list_elements)[i]);
				if (!casted.val)
				{
					casted.val = root->builder.CreateLoad(casted.pointer);
				}
				root->builder.CreateStore(casted.val, loc);
			}

			//alloc the struct for it
			auto Alloca = root->builder.CreateAlloca(t->GetLLVMType(), root->builder.getInt32(1), "init_list_to_arr_arr");

			auto size = root->builder.getInt32(list_elements->size());

			//store size
			auto size_p = root->builder.CreateGEP(Alloca, { root->builder.getInt32(0), root->builder.getInt32(0) });
			root->builder.CreateStore(size, size_p);
			
			//store data pointer to the backing data
			auto data_p = root->builder.CreateGEP(Alloca, { root->builder.getInt32(0), root->builder.getInt32(1) });
			root->builder.CreateStore(ArrayAlloc, data_p);
			
			return CValue(t, 0, Alloca);
    	}
    	else if (t->type == Types::Struct)
    	{
    		// we probably want to support this in the future
    	}
    }

	this->root->Error("Cannot cast '" + value.type->ToString() + "' to '" + t->ToString() + "'", current_token);
}

//reduce redundancy by making one general version that just omits the llvm::Value from the CValue when doign a check
bool CompilerContext::CheckCast(Type* src, Type* t, bool Explicit, bool Throw)
{
	CValue value(src, 0);

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
		this->root->Error("Cannot cast '" + value.type->ToString() + "' to '" + t->ToString() + "'!", current_token);

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
    // Destruct in backwards order
    for (int i = to_destruct.size() - 1; i >= 0; i--)
    {
        const auto& ii = to_destruct[i];
        if (ii.type->type == Types::Struct)
        {
			context->Destruct(ii, 0);
        }
        else if (ii.type->type == Types::InternalArray && ii.type->base->type == Types::Struct)
        {
            auto loc = context->root->builder.CreateGEP(ii.pointer, { context->root->builder.getInt32(0), context->root->builder.getInt32(0) });
            context->Destruct(CValue(ii.type->base, 0, loc),
                              context->root->builder.getInt32(ii.type->size));
        }
		else if (ii.type->type == Types::Array && ii.type->base->type == Types::Struct)
		{
			auto loc = context->root->builder.CreateGEP(ii.pointer, { context->root->builder.getInt32(0), context->root->builder.getInt32(0) });
			auto size = context->root->builder.CreateLoad(loc);

			auto ptr = context->root->builder.CreateGEP(ii.pointer, { context->root->builder.getInt32(0), context->root->builder.getInt32(1) });
			ptr = context->root->builder.CreateLoad(ptr);
			context->Destruct(CValue(ii.type->base->GetPointerType(), ptr), size);
		}
	}
	this->destructed = true;
}

void CompilerContext::EndStatement()
{
    for (int i = to_destruct.size() - 1; i >= 0; i--)
    {
        const auto& ii = to_destruct[i];
        if (ii.type->type == Types::Struct)
        {
			Destruct(ii, 0);
        }
        else if (ii.type->type == Types::InternalArray && ii.type->base->type == Types::Struct)
        {
            auto loc = root->builder.CreateGEP(ii.pointer, { root->builder.getInt32(0), root->builder.getInt32(0) });
            Destruct(CValue(ii.type->base, 0, loc),
                            root->builder.getInt32(ii.type->size));
        }
		else if (ii.type->type == Types::Array && ii.type->base->type == Types::Struct)
		{
			auto loc = root->builder.CreateGEP(ii.pointer, { root->builder.getInt32(0), root->builder.getInt32(0) });
			auto size = root->builder.CreateLoad(loc);

			auto ptr = root->builder.CreateGEP(ii.pointer, { root->builder.getInt32(0), root->builder.getInt32(1) });
			ptr = root->builder.CreateLoad(ptr);
			Destruct(CValue(ii.type->base->GetPointerType(), ptr), size);
		}
	}

    to_destruct.clear();
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
			elements.push_back(var.type->GetLLVMType());
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

            if (!val.val)
            {
                val.val = root->builder.CreateLoad(val.pointer);
            }
			root->builder.CreateStore(val.val, ptr);
		}
		if (size > 64)
		{
			this->root->Error("Capture size too big! Captured " + std::to_string(size) + " bytes but max was 64!", this->current_token);
		}
	}
	this->captures.clear();
}

CValue CompilerContext::Load(const std::string& name)
{
    return GetVariable(name);
}

void CompilerContext::RegisterLocal(
  const std::string& name, CValue val,
  bool needs_destruction, bool is_const)
{
    // check any above scope to see if it has the same variable defined
    auto cur_scope = this->scope;
    do 
    {
        if (cur_scope->named_values.find(name) != cur_scope->named_values.end())
        {
            if (cur_scope == this->scope)
		        this->root->Error("Variable '" + name + "' already defined", this->current_token);
            else
                this->root->Error("Variable '" + name + "' already defined in higher scope", this->current_token);
        }
        cur_scope = cur_scope->prev;
    } while (cur_scope);
	
	if (needs_destruction)
		this->scope->to_destruct.push_back(val);

    val.is_const = is_const;
	this->scope->named_values.insert( { name, val} );
}

void CompilerContext::Construct(CValue pointer, llvm::Value* arr_size)
{
	if (pointer.type->base->type == Types::Struct)
	{
		Type* ty = pointer.type->base;
		Function* fun = 0;
		if (ty->data->template_base)
			fun = ty->GetMethod("new", { pointer.type }, this);
		else
			fun = ty->GetMethod("new", { pointer.type }, this);
		if (fun == 0)
			return;//todo: is this right behavior?
		fun->Load(this->root);
		if (arr_size == 0)
		{//just one element, construct it
			this->root->builder.CreateCall(fun->f_, { pointer.val });
		}
		else
		{//construct each child element
			llvm::Value* counter = this->root->builder.CreateAlloca(this->root->IntType->GetLLVMType(), 0, "newcounter");
			this->root->builder.CreateStore(this->Integer(0).val, counter);

            auto llvm_f = root->current_function->function->f_;
			auto start = llvm::BasicBlock::Create(this->root->context, "start", llvm_f);
			auto body = llvm::BasicBlock::Create(this->root->context, "body", llvm_f);
			auto end = llvm::BasicBlock::Create(this->root->context, "end", llvm_f);

			this->root->builder.CreateBr(start);
			this->root->builder.SetInsertPoint(start);
			auto cval = this->root->builder.CreateLoad(counter, "curcount");
			auto res = this->root->builder.CreateICmpUGE(cval, arr_size);
			this->root->builder.CreateCondBr(res, end, body);

			this->root->builder.SetInsertPoint(body);
			if (pointer.type->type == Types::Array)
			{
				auto elementptr = this->root->builder.CreateGEP(pointer.val, { this->root->builder.getInt32(0), cval });
				this->root->builder.CreateCall(fun->f_, { elementptr });
			}
			else
			{
				auto elementptr = this->root->builder.CreateGEP(pointer.val, { cval });
				this->root->builder.CreateCall(fun->f_, { elementptr });
			}

			auto inc = this->root->builder.CreateAdd(cval, this->Integer(1).val);
			this->root->builder.CreateStore(inc, counter);

			this->root->builder.CreateBr(start);

			this->root->builder.SetInsertPoint(end);
		}
	}
}

void CompilerContext::SetNamespace(const std::string& name)
{
	auto res = this->root->ns->members.find(name);
	if (res != this->root->ns->members.end())
	{
        if (res->second.type == SymbolType::Type && res->second.ty->type == Types::Struct)
        {
            this->root->ns = res->second.ty->data;
        }
        else
        {
            this->root->ns = res->second.ns;
        }
		return;
	}

	//create new one
	auto ns = new Namespace(name, this->root->ns);

	this->root->ns->members.insert({ name, Symbol(ns) });
	this->root->ns = ns;
}

void CompilerContext::DestructLater(CValue data)
{
    this->to_destruct.push_back(data);
}

void CompilerContext::Destruct(CValue reference, llvm::Value* arr_size)
{
    bool is_array = reference.type->type == Types::Array;
	if ((reference.type->type == Types::Struct || is_array) && reference.pointer)
	{
		Type* ty = reference.type;
		Function* fun = 0;
		if (ty->data->template_base)
			fun = ty->GetMethod("free", { ty->GetPointerType() }, this);
		else
			fun = ty->GetMethod("free", { ty->GetPointerType() }, this);
		if (fun == 0)
			return;
		fun->Load(this->root);
		if (arr_size == 0)
		{   //just one element, destruct it
			this->root->builder.CreateCall(fun->f_, { reference.pointer });
		}
		else
		{   //destruct each child element
			llvm::Value* counter = this->root->builder.CreateAlloca(this->root->IntType->GetLLVMType(), 0, "newcounter");
			this->root->builder.CreateStore(this->Integer(0).val, counter);

            auto llvm_f = root->current_function->function->f_;
			auto start = llvm::BasicBlock::Create(this->root->context, "start", llvm_f);
			auto body = llvm::BasicBlock::Create(this->root->context, "body", llvm_f);
			auto end = llvm::BasicBlock::Create(this->root->context, "end", llvm_f);

			this->root->builder.CreateBr(start);
			this->root->builder.SetInsertPoint(start);
			auto cval = this->root->builder.CreateLoad(counter, "curcount");
			auto res = this->root->builder.CreateICmpUGE(cval, arr_size);
			this->root->builder.CreateCondBr(res, end, body);

			this->root->builder.SetInsertPoint(body);
			if (is_array)//false)//pointer.type->type == Types::Array)
			{
				auto elementptr = this->root->builder.CreateGEP(reference.pointer, { this->root->builder.getInt32(0), cval });
				this->root->builder.CreateCall(fun->f_, { elementptr });
			}
			else
			{
				auto elementptr = this->root->builder.CreateGEP(reference.pointer, { cval });
				this->root->builder.CreateCall(fun->f_, { elementptr });
			}

			auto inc = this->root->builder.CreateAdd(cval, this->Integer(1).val);
			this->root->builder.CreateStore(inc, counter);

			this->root->builder.CreateBr(start);

			this->root->builder.SetInsertPoint(end);
		}
	}
}
