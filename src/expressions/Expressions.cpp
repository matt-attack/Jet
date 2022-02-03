#include "Expressions.h"
#include "Compiler.h"
#include "Parser.h"
#include "types/Function.h"

using namespace Jet;

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DerivedTypes.h>

CValue GetPtrToExprValue(CompilerContext* context, Expression* right)
{
	auto i = dynamic_cast<NameExpression*>(right);
	auto p = dynamic_cast<IndexExpression*>(right);
	if (i)
	{
        CValue var = context->GetVariable(i->GetName());
        if (!var.pointer)
        {
          context->root->Error("Cannot get pointer", context->current_token);
        }
		return CValue(var.type->GetPointerType(), var.pointer, 0, var.is_const);
	}
	else if (p)
	{
        CValue element = p->GetElement(context);
        if (element.pointer)
        {
		    return CValue(element.type->GetPointerType(), element.pointer, 0, element.is_const);
        }
        context->root->Error("Cannot get pointer", context->current_token);
	}
	else
	{
		auto g = dynamic_cast<GroupExpression*>(right);
		if (g)
		{
			return GetPtrToExprValue(context, g->GetInside());
		}
	}
	context->root->Error("Not Implemented in GetPtrToExprValue", context->current_token);
}

CValue SliceExpression::Compile(CompilerContext* context)
{
    CValue lval = left->Compile(context);

    llvm::Value* ival;
    if (index)
    {
      ival = context->DoCast(context->root->IntType, index->Compile(context)).val;
    }
    else
    {
      ival = context->root->builder.getInt32(0);
    }

    llvm::Value* lenval;
    if (length)
    {
      lenval = context->DoCast(context->root->IntType, length->Compile(context)).val;
    }
    else
    {
      if (lval.type->type == Types::Array)
      {
        if (lval.val)
        {
          std::vector<unsigned int> iindex = { 0 };
          lenval = context->root->builder.CreateExtractValue(lval.val, iindex);
        }
        else
        {
		  auto loc = context->root->builder.CreateGEP(lval.pointer, { context->root->builder.getInt32(0), context->root->builder.getInt32(0) });
		  lenval = context->root->builder.CreateLoad(loc);
        }
      }
      else if (lval.type->type == Types::InternalArray)
      {
        lenval = context->root->builder.getInt32(lval.type->size);
      }

      // Subtract the initial index from the old length to get the correct size
      lenval = context->root->builder.CreateSub(lenval, ival);
    }

    if (lval.type->type != Types::Array && lval.type->type != Types::InternalArray)
    {
      context->root->Error("Cannot slice non-array type '" + lval.type->ToString() + "'", token);

      return CValue(context->root->VoidType, 0);
    }

    // Now alloca the array
    Type* t = context->root->GetArrayType(lval.type->base);
    llvm::Value* alloca = context->root->builder.CreateAlloca(t->GetLLVMType());

    // Now construct by getting the pointer, then the length
    llvm::Value* ptr;
    if (lval.type->type == Types::Array)
    {
      //context->root->Error("Unhandled for now", token);
      if (lval.val)
      {
        std::vector<unsigned int> iindex = { 1 };
        ptr = context->root->builder.CreateExtractValue(lval.val, iindex);
      }
      else
      {
		ptr = context->root->builder.CreateGEP(lval.pointer, { context->root->builder.getInt32(0), context->root->builder.getInt32(1) });
      }
    }
    else if (lval.type->type == Types::InternalArray)
    {
      // the ptr already is an internal array one, now do a gep to the right index
      ptr = context->root->builder.CreateGEP(lval.pointer, { context->root->builder.getInt32(0), context->root->builder.getInt32(0) });
    }

    // now add the index
    ptr = context->root->builder.CreateGEP(ptr, ival);

    // store it into the struct
    auto ptrloc = context->root->builder.CreateGEP(alloca, { context->root->builder.getInt32(0), context->root->builder.getInt32(1) });
    context->root->builder.CreateStore(ptr, ptrloc);

    auto lenloc = context->root->builder.CreateGEP(alloca, { context->root->builder.getInt32(0), context->root->builder.getInt32(0) });
    context->root->builder.CreateStore(lenval, lenloc);

    return CValue(t, context->root->builder.CreateLoad(alloca), alloca);
}

CValue PrefixExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&this->_operator);

	if (this->_operator.type == TokenType::BAnd)
    {
		CValue v = GetPtrToExprValue(context, right);

        // error if the value is const
        if (v.is_const)
            context->root->Error("Cannot get pointer to const value", _operator);

        return v;
    }

	auto rhs = right->Compile(context);

	auto res = context->UnaryOperation(this->_operator.type, rhs);
	//store here
	//only do this for ++and--
	if (this->_operator.type == TokenType::Increment || this->_operator.type == TokenType::Decrement)
		if (auto storable = dynamic_cast<IStorableExpression*>(this->right))
			storable->CompileStore(context, res);

	return res;
}

void PrefixExpression::CompileStore(CompilerContext* context, CValue right)
{
	if (_operator.type != TokenType::Asterisk)
    {
		context->root->Error("Cannot store into this expression!", _operator);
    }

	if (this->_operator.type == TokenType::Asterisk)
	{
		auto loc = this->right->Compile(context);

		context->Store(CValue(loc.type, loc.val), right);

		return;
	}
	context->root->Error("Unimplemented!", _operator);
}

CValue SizeofExpression::Compile(CompilerContext* context)
{
	auto t = context->root->LookupType(type.text);

	return context->GetSizeof(t);
}

CValue TypeofExpression::Compile(CompilerContext* context)
{
	return CValue(this->arg->TypeCheck(context), 0);
}

CValue PostfixExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&this->_operator);
	auto lhs = left->Compile(context);

    // load it pre-operation
    if (!lhs.val)
    {
        lhs.val = context->root->builder.CreateLoad(lhs.pointer, "postfix-dereference");
        lhs.pointer = 0;
    }

	auto res = context->UnaryOperation(this->_operator.type, lhs);

	//only do this for ++ and --
	if (this->_operator.type == TokenType::Increment || this->_operator.type == TokenType::Decrement)
		if (auto storable = dynamic_cast<IStorableExpression*>(this->left))	//store here
			storable->CompileStore(context, res);

	return lhs;
}

CValue IndexExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&token);
	context->SetDebugLocation(this->token);
	
	auto elm = this->GetElement(context);
	if (elm.type->type == Types::Function)
    {
		return elm;
    }

    if (!elm.val)
    {
        return CValue(elm.type, context->root->builder.CreateLoad(elm.pointer), elm.pointer);
    }
	return elm;
}

void IndexExpression::CompileStore(CompilerContext* context, CValue right)
{
	auto oldtok = context->current_token;
	context->CurrentToken(&token);
	context->SetDebugLocation(this->token);

    // We can only store into elements we can get a pointer to
	auto element = this->GetElement(context, true);
    if (!element.pointer)
    {
      context->root->Error("Cannot store into type", token);
    }
    // turn it into a pointer CValue for the store
    element = CValue(element.type->GetPointerType(), element.pointer);

	context->CurrentToken(oldtok);

	context->Store(element, right);
}

//ok, idea
//each expression will store type data in it loaded during typecheck
//compiling will only emit instructions, it should do little actual work
Type* IndexExpression::GetBaseType(CompilerContext* context, bool tc)
{
	context->CurrentToken(&this->token);
	if (auto name = dynamic_cast<NameExpression*>(left))
		if (tc)
			return context->TCGetVariable(name->GetName())->base;
		else
			return context->GetVariable(name->GetName()).type->base;
	else if (auto index = dynamic_cast<IndexExpression*>(left))
		return index->GetType(context, tc);
	else if (auto call = dynamic_cast<CallExpression*>(left))
		return call->TypeCheck(context);//have function call expression get data during typechecking

	context->root->Error("Unexpected error.", token);
}

Type* IndexExpression::GetBaseType(Compilation* compiler)
{
	if (auto p = dynamic_cast<NameExpression*>(left))
	{
		//look for neareast scope
		auto current = this->parent;
		do
		{
			auto scope = dynamic_cast<ScopeExpression*>(current);
			if (scope)
			{
				//search for it now
				auto curscope = scope->scope;
				do
				{
					auto res = curscope->named_values.find(p->GetName());
					if (res != curscope->named_values.end())
						return res->second.type;
				} while (curscope = curscope->prev);
				break;
			}
		} while (current = current->parent);
	}
	else if (auto i = dynamic_cast<IndexExpression*>(left))
	{
		compiler->Error("todo", token);
	}

	compiler->Error("wat", token);
}

CValue IndexExpression::GetBaseElement(CompilerContext* context)
{
	if (auto name = dynamic_cast<NameExpression*>(left))
	{
		return context->GetVariable(name->GetName());
	}
	else if (auto index = dynamic_cast<IndexExpression*>(left))
	{
		return index->GetElement(context, false);
	}
	else if (auto call = dynamic_cast<CallExpression*>(left))
	{
		//return a modifiable pointer to the value
		return call->Compile(context);
	}
	context->root->Error("Could not handle Get Base Element", token);
}

Type* GetMemberType(CompilerContext* context, Type* type, const std::string& name, const Token& token)
{
	int index = 0;
	for (; index < type->data->struct_members.size(); index++)
	{
		if (type->data->struct_members[index].name == name)
			break;
	}
	if (index >= type->data->struct_members.size())
	{
		//check methods
		auto method = type->GetMethod(name, {}, context, true);
		if (method == 0)
		{
			context->root->Error("Struct Member '" + name + "' of Struct '" + type->data->name + "' Not Found", token);
		}
		return method->GetType(context->root);
	}

	if (index >= type->data->struct_members.size())
	{
		context->root->Error("Struct Member '" + name + "' of Struct '" + type->data->name + "' Not Found", token);
	}

	return type->data->struct_members[index].type;
}

Type* IndexExpression::GetType(CompilerContext* context, bool tc)
{
	auto p = dynamic_cast<NameExpression*>(left);
	auto i = dynamic_cast<IndexExpression*>(left);
	//bad implementation
	auto string = dynamic_cast<StringExpression*>(index);
	if (p || i)
	{
		CValue lhs(0, 0);
		if (p)
		{
			if (tc)
			{
				lhs.type = context->TCGetVariable(p->GetName());
			}
			else
			{
				lhs = context->GetVariable(p->GetName());
			}
		}
		else if (i)
		{
			lhs.type = i->GetType(context, tc)->GetPointerType();
		}

		if (string && lhs.type->type == Types::Struct)
		{
			return GetMemberType(context, lhs.type, string->GetValue(), this->token);
		}
		else if (this->token.type == TokenType::Dot && this->member.text.length() && lhs.type->type == Types::Pointer && lhs.type->base->type == Types::Struct)
		{
			return GetMemberType(context, lhs.type->base, this->member.text, this->token);
		}
		else if (this->token.type == TokenType::Pointy && this->member.text.length() && lhs.type->type == Types::Pointer && lhs.type->base->type == Types::Pointer && lhs.type->base->base->type == Types::Struct)
		{
			unsigned int index = 0;
			for (; index < lhs.type->base->base->data->struct_members.size(); index++)
			{
				if (lhs.type->base->base->data->struct_members[index].name == this->member.text)
					break;
			}

			if (index >= lhs.type->base->base->data->struct_members.size())
			{
				//check functions
				for (auto ii : lhs.type->base->base->data->functions)
				{
					if (ii.first == this->member.text)
						return ii.second->GetType(context->root);
				}
				context->root->Error("Struct Member '" + this->member.text + "' of Struct '" + lhs.type->base->base->data->name + "' Not Found", this->member);
			}
			if (tc && lhs.type->base->base->data->struct_members[index].type == 0)
				return context->root->LookupType(lhs.type->base->base->data->struct_members[index].type_name, !tc);

			return lhs.type->base->base->data->struct_members[index].type;
		}
		else if ((lhs.type->type == Types::Array || lhs.type->type == Types::Pointer) && (lhs.type->base->type == Types::Pointer || lhs.type->base->type == Types::Array) && string == 0)//or pointer!!(later)
		{
			if (lhs.type->base->base == 0 && this->member.text.length())
				context->root->Error("Cannot access member '" + this->member.text + "' of type '" + lhs.type->base->ToString() + "'", this->member);

			return lhs.type->base->base;
		}
		else
		{
			auto stru = lhs.type->base->data;

			auto funiter = stru->functions.find("[]");
			//todo: search through multimap to find one with the right number of args
			if (funiter != stru->functions.end() && funiter->second->arguments_.size() == 2)
			{
				Function* fun = funiter->second;
				//CValue right = index->Compile(context);
				//todo: casting
				return fun->return_type_->base;
			}
		}
	}
	throw 7;//error if we miss it
}

CValue GetStructElement(CompilerContext* context, const std::string& name, const Token& token, Type* type, llvm::Value* lhs)
{
	unsigned int index = 0;
	for (; index < type->data->struct_members.size(); index++)
	{
		if (type->data->struct_members[index].name == name)
			break;
	}
	if (index >= type->data->struct_members.size())
	{
		//check methods
		auto method = type->GetMethod(name, {}, context, true);
		if (method == 0)
			context->root->Error("Struct Member '" + name + "' of Struct '" + type->data->name + "' Not Found", token);
		method->Load(context->root);
		return CValue(method->GetType(context->root), method->f_);
	}
	std::vector<llvm::Value*> iindex = { context->root->builder.getInt32(0), context->root->builder.getInt32(index) };

	auto loc = context->root->builder.CreateGEP(lhs, iindex, "index");
	return CValue(type->data->struct_members[index].type, 0, loc);
}

// Returns either the value or pointer to the element.
// Pointer is preferred, but some things are values and have no pointer to them
CValue IndexExpression::GetElement(CompilerContext* context, bool for_store)
{
	auto p = dynamic_cast<NameExpression*>(left);
	auto i = dynamic_cast<IndexExpression*>(left);

    // First get the left hand side
	CValue lhs(0, 0);
	if (p)
	{
		auto old = context->current_token;
		context->CurrentToken(&p->token);
		lhs = context->GetVariable(p->GetName());
        if (lhs.is_const && for_store && lhs.type->type == Types::Struct)
        {
           context->root->Error("Cannot store into const value '" + p->GetName() + "'", p->token);
        }
		context->CurrentToken(old);
	}
	else if (i)
	{
		lhs = i->GetElement(context, for_store);
	}
    else
    {
        // otherwise just compile
        lhs = left->Compile(context);
        if (lhs.is_const && for_store && lhs.type->type == Types::Struct)
        {
           context->root->Error("Cannot store into const value", p->token);
        }
    }

    // Now index into it using the index or member name
	if (lhs.type->type == Types::Struct)
    {
        if (this->member.text.length())
        {
            return GetStructElement(context, this->member.text, this->member, lhs.type, lhs.pointer);
        }
        else if (lhs.pointer)// we're indexing it
		{
			auto stru = lhs.type->data;

			// todo maybe wrap finding this function into a function
			auto funiter = stru->functions.find("[]");
			//todo: search through multimap to find one with the right number of args
			if (funiter != stru->functions.end() && funiter->second->arguments_.size() == 2)
			{
				Function* fun = funiter->second;
				fun->Load(context->root);
				CValue right = index->Compile(context);
				//todo: casting
				return CValue(fun->return_type_, context->root->builder.CreateCall(fun->f_, { lhs.pointer, right.val }, "operator_overload"));
			}
            else
            {
                context->root->Error("Cannot index struct '" + lhs.type->name + "'.", this->token);
            }
        }
    }
    else if (lhs.type->type == Types::Array)
    {
        if (this->member.text.length() == 0)//or pointer!!(later)
		{
            if (!lhs.val)
            {
                lhs.val = context->root->builder.CreateLoad(lhs.pointer, "autodereference");
            }

		    auto indexv = context->DoCast(context->root->IntType, index->Compile(context));
            if (!indexv.val)
            {
                indexv.val = context->root->builder.CreateLoad(indexv.pointer, "autodereference");
            }

            std::vector<unsigned int> iindex = { 1 };
            auto pointer = context->root->builder.CreateExtractValue(lhs.val, iindex);
			auto loc = context->root->builder.CreateGEP(pointer, indexv.val, "index");
			return CValue(lhs.type->base, 0, loc);
		}
		else
		{
            if (for_store && !lhs.pointer)
            {
              context->root->Error("Cannot modify constant array", this->token);
            }

            // if we have a pointer, we can actually GEP
            if (lhs.pointer)
            {
			    if (this->member.text == "size")
			    {
				    auto loc = context->root->builder.CreateGEP(lhs.pointer, { context->root->builder.getInt32(0), context->root->builder.getInt32(0) });
			  	    return CValue(context->root->IntType, 0, loc);
			    }
			    else if (this->member.text == "ptr")
			    {
				    auto loc = context->root->builder.CreateGEP(lhs.pointer, { context->root->builder.getInt32(0), context->root->builder.getInt32(1) });
				    return CValue(lhs.type->base->GetPointerType(), 0, loc);
			    }
            }
            // otherwise just extract the values
            else
            {
			    if (this->member.text == "size")
			    {
                    std::vector<unsigned int> iindex = { 0 };
                    auto size = context->root->builder.CreateExtractValue(lhs.val, iindex);
				    return CValue(context->root->IntType, size);
			    }
			    else if (this->member.text == "ptr")
			    {
                    std::vector<unsigned int> iindex = { 1 };
                    auto ptr = context->root->builder.CreateExtractValue(lhs.val, iindex);
				    return CValue(lhs.type->base->GetPointerType(), ptr);
			    }
            }
		}
    }
    else if (lhs.type->type == Types::InternalArray)
    {
        // Note, we _always_ have the pointer to an internal array type.
        // They only exist as locals and struct members (and we have no true value only structs)

        // If accessing size or ptr return values for them since we dont actually store these
		if (this->member.text.length() != 0)
		{
			if (this->member.text == "size")
			{
				if (for_store)
				{
					context->root->Error("Cannot modify fixed array size", this->token);
				}

				return CValue(context->root->IntType, context->root->builder.getInt32(lhs.type->size));
			}
			else if (this->member.text == "ptr")
			{
				/*if (for_store)
				{
					context->root->Error("Cannot modify fixed array ptr", this->token);
				}*/

				//just return a pointer to myself cast by GEP to the base type
				auto loc = context->root->builder.CreateGEP(lhs.pointer, { context->root->builder.getInt32(0), context->root->builder.getInt32(0) });

				return CValue(lhs.type->base->GetPointerType(), loc);
			}
		}
        // Indexing into it
        else
		{
            auto indexv = context->DoCast(context->root->IntType, index->Compile(context));
            if (!indexv.val)
            {
                indexv.val = context->root->builder.CreateLoad(indexv.pointer, "autodereference");
            }
			std::vector<llvm::Value*> iindex = { context->root->builder.getInt32(0), indexv.val};

			auto loc = context->root->builder.CreateGEP(lhs.pointer, iindex, "array_index");

			return CValue(lhs.type->base, 0, loc);
		}
    }
	else if (lhs.type->type == Types::Pointer)
	{
        // autodereference if we havent already
        if (!lhs.val)
        {
            lhs.val = context->root->builder.CreateLoad(lhs.pointer, "autodereference");
        }

        // Auto dereference struct pointers
		if (this->member.text.length() && lhs.type->base->type == Types::Struct)
		{
			return GetStructElement(context, this->member.text, this->member, lhs.type->base, lhs.val);
		}
        // Auto dereference array pointers if accessing size or ptr to give the same behavior as structs
		else if (this->member.text.length() && lhs.type->base->type == Types::Array)
		{
			if (this->member.text == "size")
			{
				auto loc = context->root->builder.CreateGEP(lhs.val, { context->root->builder.getInt32(0), context->root->builder.getInt32(0) });
				return CValue(context->root->IntType, 0, loc);
			}
			else if (this->member.text == "ptr")
			{
				auto loc = context->root->builder.CreateGEP(lhs.val, { context->root->builder.getInt32(0), context->root->builder.getInt32(1) });
				return CValue(lhs.type->base->base->GetPointerType(), 0, loc);
			}
        }
        // If we have a pointer, allow indexing it C style
		else if (this->member.text.length() == 0)
		{
            CValue indexv = context->DoCast(context->root->IntType, index->Compile(context));
            if (!indexv.val)
            {
                indexv.val = context->root->builder.CreateLoad(indexv.pointer);
            }
			std::vector<llvm::Value*> iindex = { indexv.val };

			auto loc = context->root->builder.CreateGEP(lhs.val, iindex, "pointer_index");

			return CValue(lhs.type->base, 0, loc);
		}
	}

	context->root->Error("Unimplemented Get Element for " + lhs.type->ToString(), this->token);
}

CValue StringExpression::Compile(CompilerContext* context)
{
	return context->String(this->value);
}

NumberExpression::Number NumberExpression::GetValue()
{
	bool isint = true;
	bool ishex = false;
	bool isfloat = false;
	for (unsigned int i = 0; i < this->token.text.length(); i++)
	{
		if (this->token.text[i] == '.')
			isint = false;
	}

	if (token.text.back() == 'f')
	{
		isfloat = true;
	}
	else if (token.text.length() >= 3)
	{
		std::string substr = token.text.substr(2);
		if (token.text[1] == 'x')
		{
			unsigned long long num = std::stoull(substr, nullptr, 16);
			Number n;
			n.type = Number::Int;
			n.data.i = num;
			return n;
		}
		else if (token.text[1] == 'b')
		{
			unsigned long long num = std::stoull(substr, nullptr, 2);
			Number n;
			n.type = Number::Int;
			n.data.i = num;
			return n;
		}
	}

	//ok, lets get the type from what kind of constant it is
	//get type from the constant
	//this is pretty terrible, come back later

	Number n;
	if (isint)
	{
		n.type = Number::Int;
		n.data.i = std::stoi(this->token.text);
	}
	else if (isfloat)
	{
		n.type = Number::Float;
		n.data.f = atof(token.text.substr(0, token.text.length() - 1).c_str());
	}
	else
	{
		n.type = Number::Double;
		n.data.d = atof(token.text.c_str());
	}
	return n;
}

CValue NumberExpression::Compile(CompilerContext* context)
{
	Number n = this->GetValue();
	if (n.type == Number::Double)
	{
		return context->Double(n.data.d);
	}
	else if (n.type == Number::Float)
	{
		return context->Float(n.data.f);
	}
	else if (n.type == Number::Int)
	{
		// todo handle different sizes of integers? Everything is 32 bit now
		return context->Integer(n.data.i);
	}
	else
	{
		context->root->Error("Compiler error: unhandled NumberExpression GetValue() type.", this->token);// compiler error
	}
}

CValue AssignExpression::Compile(CompilerContext* context)
{
	context->SetDebugLocation(this->token);

	context->CurrentToken(&this->token);

	if (auto storable = dynamic_cast<IStorableExpression*>(this->left))
		storable->CompileStore(context, right->Compile(context));

	return CValue(context->root->VoidType, 0);
}

CValue NameExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&token);

    if (namespaced)
    {
        // Get the variable or function from the namespace
        auto sym = context->root->GetVariableOrFunction(token.text);

		if (sym.type != SymbolType::Invalid)
		{
			if (sym.type == SymbolType::Function)
			{
				auto function = sym.fn;
				function->Load(context->root);
				return CValue(function->GetType(context->root), function->f_);
			}
			else if (sym.type == SymbolType::Variable)
			{
				//variable
				Jet::CValue val = *sym.val;

                return val;
			}
		}
        else
        {
            context->root->Error("Undeclared identifier '" + this->token.text + "'", this->token);
        }
    }


	return context->Load(token.text);
}

CValue OperatorAssignExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&token);
	context->SetDebugLocation(token);

	//try and cast right side to left
	auto lhs = this->left->Compile(context);
	auto rhs = this->right->Compile(context);

	context->CurrentToken(&token);
	auto res = context->BinaryOperation(token.type, lhs, rhs);

	if (auto storable = dynamic_cast<IStorableExpression*>(this->left))
		storable->CompileStore(context, res);
	else
		context->root->Error("Cannot store into this type.", this->token);

	return CValue(context->root->VoidType, 0);
}

CValue OperatorExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&this->_operator);
	context->SetDebugLocation(this->_operator);

	// Handle short circuiting
	if (this->_operator.type == TokenType::And)
	{
		auto else_block = llvm::BasicBlock::Create(context->context, "land.shortcircuitelse");
		auto end_block = llvm::BasicBlock::Create(context->context, "land.endshortcircuit");

		auto cond = this->left->Compile(context);
		cond = context->DoCast(context->root->BoolType, cond);

        auto f = context->function->f_;

		auto cur_block = context->root->builder.GetInsertBlock();
		context->root->builder.CreateCondBr(cond.val, else_block, end_block);

		f->getBasicBlockList().push_back(else_block);
		context->root->builder.SetInsertPoint(else_block);
		auto cond2 = this->right->Compile(context);

		cond2 = context->DoCast(context->root->BoolType, cond2);
		context->root->builder.CreateBr(end_block);

		f->getBasicBlockList().push_back(end_block);
		context->root->builder.SetInsertPoint(end_block);
		auto phi = context->root->builder.CreatePHI(cond.type->GetLLVMType(), 2, "land");
		phi->addIncoming(cond.val, cur_block);
		phi->addIncoming(cond2.val, else_block);

		return CValue(context->root->BoolType, phi);
	}
	else if (this->_operator.type == TokenType::Or)
	{
		auto else_block = llvm::BasicBlock::Create(context->context, "lor.shortcircuitelse");
		auto end_block = llvm::BasicBlock::Create(context->context, "lor.endshortcircuit");

		auto cond = this->left->Compile(context);
		cond = context->DoCast(context->root->BoolType, cond);

        auto f = context->function->f_;

		auto cur_block = context->root->builder.GetInsertBlock();
		context->root->builder.CreateCondBr(cond.val, end_block, else_block);

		f->getBasicBlockList().push_back(else_block);
		context->root->builder.SetInsertPoint(else_block);
		auto cond2 = this->right->Compile(context);
		cond2 = context->DoCast(context->root->BoolType, cond2);
		context->root->builder.CreateBr(end_block);

		f->getBasicBlockList().push_back(end_block);
		context->root->builder.SetInsertPoint(end_block);

		auto phi = context->root->builder.CreatePHI(cond.type->GetLLVMType(), 2, "lor");
		phi->addIncoming(cond.val, cur_block);
		phi->addIncoming(cond2.val, else_block);
		return CValue(context->root->BoolType, phi);
	}

	// Handle non-short-circuiting binary operations
	auto lhs = this->left->Compile(context);

	auto rhs = this->right->Compile(context);
    
	context->CurrentToken(&this->_operator);
	return context->BinaryOperation(this->_operator.type, lhs, rhs);
}

CValue NewExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&this->type);

	auto ty = context->root->LookupType(type.text);

	context->CurrentToken(&this->token);
	auto size = context->GetSizeof(ty);
	auto arr_size = this->size ? this->size->Compile(context).val : context->root->builder.getInt32(1);
	
	// todo watch out for this constant 4 for the size, limits char arrays to 4gb
	if (this->size)
		size.val = context->root->builder.CreateMul(size.val, arr_size);
	size.val = context->root->builder.CreateAdd(size.val, context->root->builder.getInt32(4));
	
	//ok now get new working with it
    Token fname;
    fname.text = "malloc";
	CValue val = context->Call(fname, { size });// todo lets not use full on call here

	auto pointer = context->root->builder.CreatePointerCast(val.val, context->root->IntType->GetPointerType()->GetLLVMType());
	context->root->builder.CreateStore(arr_size, pointer);
	val.val = context->root->builder.CreateGEP(val.val, { context->root->builder.getInt32(4) });

	auto ptr = context->DoCast(ty->GetPointerType(), val, true);

	//run constructors
	if (this->args)
	{
		if (ptr.type->base->type == Types::Struct)
		{
			std::vector<llvm::Value*> llvm_args = { ptr.val };
			std::vector<Type*> arg_types = { ptr.type };
			for (auto ii : *this->args)
			{
				auto res = ii.first->Compile(context);
				//need to try and do casts
				llvm_args.push_back(res.val);
				arg_types.push_back(res.type);
			}
			Type* ty = ptr.type->base;
			Function* fun = 0;
			if (ty->data->template_base)
			{
				fun = ty->GetMethod(ty->data->template_base->name, arg_types, context);
			}
			else
			{
				fun = ty->GetMethod(ty->data->name, arg_types, context);
			}

			if (fun == 0)
			{
				std::string err = "Constructor for '";
				err += ty->data->template_base ? ty->data->template_base->name : ty->data->name;
				err += "' with arguments (";
				unsigned int i = 0;
				for (auto ii : arg_types)
				{
					err += ii->ToString();
					if (i++ < (arg_types.size() - 1))
					{
						err += ',';
					}
				}
				err += ") not found!";
				context->root->Error(err, this->token);
			}
			fun->Load(context->root);
			context->root->builder.CreateCall(fun->f_, llvm_args);
		}
		else
		{
			context->root->Error("Cannot construct non-struct type!", this->token);
		}

		//handle constructor args
		return ptr;
	}

	context->Construct(ptr, this->size ? arr_size : 0);

	//ok now stick it in an array if we specified size
	if (this->size)
	{
		auto str_type = context->root->GetArrayType(ty);
		//alloc the struct for it
		auto str = context->root->builder.CreateAlloca(str_type->GetLLVMType(), context->root->builder.getInt32(1), "newarray");
		
		//store size
		auto size_p = context->root->builder.CreateGEP(str, { context->root->builder.getInt32(0), context->root->builder.getInt32(0) });
		context->root->builder.CreateStore(arr_size, size_p);

		//store pointer
		auto pointer_p = context->root->builder.CreateGEP(str, { context->root->builder.getInt32(0), context->root->builder.getInt32(1) });
		context->root->builder.CreateStore(ptr.val, pointer_p);
		
		auto strv = context->root->builder.CreateLoad(str);
		return CValue(str_type, strv);
	}
	return ptr;
}

CValue FreeExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&this->token);

	auto pointer = this->pointer->Compile(context);

	if (pointer.type->type == Types::InternalArray)
	{
		context->root->Error("Cannot free this type of array.", this->token);
	}
	else if (pointer.type->type != Types::Pointer && pointer.type->type != Types::Array)
	{
		context->root->Error("Cannot free a non pointer/array type!", this->token);
	}

	//extract the pointer from the array if we are one
	if (pointer.type->type == Types::Array)
	{
		auto ptr = context->root->builder.CreateExtractValue(pointer.val, 1);
		pointer = CValue(pointer.type->base->GetPointerType(), ptr);
	}

	//get to the root of the pointer (remove the offset for the size)
	llvm::Value* charptr = context->root->builder.CreatePointerCast(pointer.val, context->root->builder.getInt8PtrTy());
	llvm::Value* rootptr = context->root->builder.CreateGEP(charptr, { context->root->builder.getInt32(-4) });// todo make this smarter about 64 bit

	//run destructors
	if (pointer.type->base->type == Types::Struct)
	{
		Type* ty = pointer.type->base;
		Function* fun = 0;
		if (ty->data->template_base)
		{
			fun = ty->GetMethod("~" + ty->data->template_base->name, { pointer.type }, context);
		}
		else
		{
			fun = ty->GetMethod("~" + ty->data->name, { pointer.type }, context);
		}

		if (fun)
		{
			fun->Load(context->root);
			if (false)//this->close_bracket.text.length() == 0)//size == 0)
			{//just one element, destruct it
				rootptr = context->root->builder.CreatePointerCast(pointer.val, context->root->builder.getInt8PtrTy());

				context->root->builder.CreateCall(fun->f_, { pointer.val });
			}
			else
			{
				auto arr_size = context->root->builder.CreateLoad(context->root->builder.CreatePointerCast(rootptr, context->root->IntType->GetPointerType()->GetLLVMType()));

				//destruct each child element
				llvm::Value* counter = context->root->builder.CreateAlloca(context->root->IntType->GetLLVMType(), 0, "newcounter");
				context->root->builder.CreateStore(context->Integer(0).val, counter);

                auto f = context->root->current_function->function->f_;

				auto start = llvm::BasicBlock::Create(context->root->context, "start", f);
				auto body = llvm::BasicBlock::Create(context->root->context, "body", f);
				auto end = llvm::BasicBlock::Create(context->root->context, "end", f);

				context->root->builder.CreateBr(start);
				context->root->builder.SetInsertPoint(start);
				auto cval = context->root->builder.CreateLoad(counter, "curcount");
				auto res = context->root->builder.CreateICmpUGE(cval, arr_size);
				context->root->builder.CreateCondBr(res, end, body);

				context->root->builder.SetInsertPoint(body);
				auto elementptr = context->root->builder.CreateGEP(pointer.val, { cval });
				context->root->builder.CreateCall(fun->f_, { elementptr });

				auto inc = context->root->builder.CreateAdd(cval, context->Integer(1).val);
				context->root->builder.CreateStore(inc, counter);

				context->root->builder.CreateBr(start);

				context->root->builder.SetInsertPoint(end);
			}
		}
	}

    Token fname;
    fname.text = "free";
	context->Call(fname, { CValue(context->root->CharPointerType, rootptr) });

	//todo: can mark the size and pointer as zero now

	return CValue(context->root->VoidType, 0);
}


Type* PrefixExpression::TypeCheck(CompilerContext* context)
{
	auto type = this->right->TypeCheck(context);
	//prefix
	context->CurrentToken(&this->_operator);

	if (this->_operator.type == TokenType::BAnd)
	{
		auto i = dynamic_cast<NameExpression*>(right);
		auto p = dynamic_cast<IndexExpression*>(right);
		if (i)
			return context->TCGetVariable(i->GetName());
		else if (p)
			return p->GetType(context, true);
		context->root->Error("Not Implemented", this->_operator);
	}

	//auto rhs = right->Compile(context);

	//auto res = context->UnaryOperation(this->_operator.type, rhs);

	///llvm::Value* res = 0;

	if (type->type == Types::Float || type->type == Types::Double)
	{
		switch (this->_operator.type)
		{
		case TokenType::Minus:
			//res = root->builder.CreateFNeg(value.val);// root->builder.CreateFMul(left.val, right.val);
			break;
		default:
			context->root->Error("Invalid Unary Operation '" + TokenToString[this->_operator.type] + "' On Type '" + type->ToString() + "'", context->current_token);
			break;
		}

		return type;// CValue(value.type, res);
	}
	else if (type->type == Types::Int || type->type == Types::Short || type->type == Types::Char)
	{
		//integer probably
		switch (this->_operator.type)
		{
		case TokenType::Increment:
			//res = root->builder.CreateAdd(value.val, root->builder.getInt32(1));
			break;
		case TokenType::Decrement:
			//res = root->builder.CreateSub(value.val, root->builder.getInt32(1));
			break;
		case TokenType::Minus:
			//res = root->builder.CreateNeg(value.val);
			break;
		case TokenType::BNot:
			//res = root->builder.CreateNot(value.val);
			break;
		default:
			context->root->Error("Invalid Unary Operation '" + TokenToString[this->_operator.type] + "' On Type '" + type->ToString() + "'", context->current_token);
			break;
		}

		return type;
	}
	else if (type->type == Types::Pointer)
	{
		switch (this->_operator.type)
		{
		case TokenType::Asterisk:
		case TokenType::Increment:
		case TokenType::Decrement:
			return type->base;
		default:
			;
		}

	}
	else if (type->type == Types::Bool)
	{
		switch (this->_operator.type)
		{
		case TokenType::Not:
			return type;
		default:
			break;
		}
	}
	context->root->Error("Invalid Unary Operation '" + TokenToString[this->_operator.type] + "' On Type '" + type->ToString() + "'", context->current_token);
	//store here
	//only do this for ++and--
	return type;
}
