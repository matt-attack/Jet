#include "Expressions.h"
#include "Compiler.h"
#include "Parser.h"
#include "Types/Function.h"

using namespace Jet;

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DerivedTypes.h>



CValue PrefixExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&this->_operator);

	if (this->_operator.type == TokenType::BAnd)
	{
		auto i = dynamic_cast<NameExpression*>(right);
		auto p = dynamic_cast<IndexExpression*>(right);
		if (i)
		{
			auto var = context->GetVariable(i->GetName());
			return CValue(var.type, var.val);
		}
		else if (p)
		{
			auto var = p->GetElementPointer(context);
			return CValue(var.type, var.val);
		}
		context->root->Error("Not Implemented", this->_operator);
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

CValue SizeofExpression::Compile(CompilerContext* context)
{
	auto t = context->root->LookupType(type.text);

	return context->GetSizeof(t);
}

CValue PostfixExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&this->_operator);
	auto lhs = left->Compile(context);

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
	auto loc = this->GetElementPointer(context);
	if (loc.type->type == Types::Function)
		return loc;
	return CValue(loc.type->base, context->root->builder.CreateLoad(loc.val));
}
//ok, idea
//each expression will store type data in it loaded during typecheck
//compiling will only emit instructions, it should do little actual work
Type* IndexExpression::GetBaseType(CompilerContext* context, bool tc)
{
	auto p = dynamic_cast<NameExpression*>(left);
	auto i = dynamic_cast<IndexExpression*>(left);

	if (p)
		if (tc)
			return context->TCGetVariable(p->GetName())->base;
		else
			return context->GetVariable(p->GetName()).type->base;
	else if (i)
		return i->GetType(context, tc);
	else if (auto c = dynamic_cast<CallExpression*>(left))
	{
		//fix this
		//have function call expression get data during typechecking
		context->root->Error("Chaining function calls not yet implemented", token);
		//return c->Compile(context).type;
	}
	context->root->Error("wat", token);
}

Type* IndexExpression::GetBaseType(Compilation* compiler)
{
	auto p = dynamic_cast<NameExpression*>(left);
	auto i = dynamic_cast<IndexExpression*>(left);

	if (p)
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
	//return context->GetVariable(p->GetName()).type;
	else if (i)
	{
		compiler->Error("todo", token);//return i->GetType(context);
	}

	compiler->Error("wat", token);
}

CValue IndexExpression::GetBaseElementPointer(CompilerContext* context)
{
	auto p = dynamic_cast<NameExpression*>(left);
	auto i = dynamic_cast<IndexExpression*>(left);

	if (p)
		return context->GetVariable(p->GetName());
	else if (i)
		return i->GetElementPointer(context);

	context->root->Error("wat", token);
}

Type* IndexExpression::GetType(CompilerContext* context, bool tc)
{
	auto p = dynamic_cast<NameExpression*>(left);
	auto i = dynamic_cast<IndexExpression*>(left);

	auto string = dynamic_cast<StringExpression*>(index);
	if (p || i)
	{
		CValue lhs;
		if (p)
			if (tc)
				lhs.type = context->TCGetVariable(p->GetName());
			else
				lhs = context->GetVariable(p->GetName());
		else if (i)
			lhs.type = i->GetType(context, tc)->GetPointerType();// i->GetElementPointer(context);

		if (string && lhs.type->type == Types::Struct)
		{
			int index = 0;
			for (; index < lhs.type->data->struct_members.size(); index++)
			{
				if (lhs.type->data->struct_members[index].name == string->GetValue())
					break;
			}

			if (index >= lhs.type->data->struct_members.size())
				context->root->Error("Struct Member '" + string->GetValue() + "' of Struct '" + lhs.type->data->name + "' Not Found", this->token);

			return lhs.type->data->struct_members[index].type;
		}
		else if (this->token.type == TokenType::Dot && this->member.text.length() && lhs.type->type == Types::Pointer && lhs.type->base->type == Types::Struct)
		{
			int index = 0;
			for (; index < lhs.type->base->data->struct_members.size(); index++)
			{
				if (lhs.type->base->data->struct_members[index].name == this->member.text)
					break;
			}

			if (index >= lhs.type->base->data->struct_members.size())
				context->root->Error("Struct Member '" + this->member.text + "' of Struct '" + lhs.type->base->data->name + "' Not Found", this->member);

			return lhs.type->base->data->struct_members[index].type;
		}
		else if (this->token.type == TokenType::Pointy && this->member.text.length() && lhs.type->type == Types::Pointer && lhs.type->base->type == Types::Pointer && lhs.type->base->base->type == Types::Struct)
		{
			int index = 0;
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
		else if ((lhs.type->type == Types::Array || lhs.type->type == Types::Pointer) && string == 0)//or pointer!!(later)
		{
			return lhs.type->base->base;
		}
	}
}

CValue IndexExpression::GetElementPointer(CompilerContext* context)
{
	auto p = dynamic_cast<NameExpression*>(left);
	auto i = dynamic_cast<IndexExpression*>(left);

	if (index == 0 && this->token.type == TokenType::Pointy)
	{
		CValue lhs;
		if (p)
			lhs = context->GetVariable(p->GetName());
		else if (i)
			lhs = i->GetElementPointer(context);

		if (lhs.type->type == Types::Pointer && lhs.type->base->type == Types::Pointer && lhs.type->base->base->type == Types::Struct)
		{
			lhs.val = context->root->builder.CreateLoad(lhs.val);

			auto type = lhs.type->base->base;
			int index = 0;
			for (; index < type->data->struct_members.size(); index++)
			{
				if (type->data->struct_members[index].name == this->member.text)
					break;
			}
			if (index >= type->data->struct_members.size())
			{
				//check methods
				auto method = type->GetMethod(this->member.text, {}, context, true);
				if (method == 0)
					context->root->Error("Struct Member '" + this->member.text + "' of Struct '" + type->data->name + "' Not Found", this->member);
				method->Load(context->root);
				return CValue(method->GetType(context->root), method->f);
			}

			std::vector<llvm::Value*> iindex = { context->root->builder.getInt32(0), context->root->builder.getInt32(index) };

			auto loc = context->root->builder.CreateGEP(lhs.val, iindex, "index");
			return CValue(type->data->struct_members[index].type->GetPointerType(), loc);
		}

		context->root->Error("unimplemented!", this->token);
	}
	else if (p || i)
	{
		CValue lhs;
		if (p)
			lhs = context->GetVariable(p->GetName());
		else if (i)
			lhs = i->GetElementPointer(context);

		if (this->member.text.length() && lhs.type->type == Types::Pointer && lhs.type->base->type == Types::Struct)
		{
			int index = 0;
			for (; index < lhs.type->base->data->struct_members.size(); index++)
			{
				if (lhs.type->base->data->struct_members[index].name == this->member.text)
					break;
			}
			if (index >= lhs.type->base->data->struct_members.size())
			{
				//check methods
				auto method = lhs.type->base->GetMethod(this->member.text, {}, context, true);
				if (method == 0)
					context->root->Error("Struct Member '" + this->member.text + "' of Struct '" + lhs.type->base->data->name + "' Not Found", this->member);
				return CValue(method->GetType(context->root), method->f);
			}
			std::vector<llvm::Value*> iindex = { context->root->builder.getInt32(0), context->root->builder.getInt32(index) };

			auto loc = context->root->builder.CreateGEP(lhs.val, iindex, "index");
			return CValue(lhs.type->base->data->struct_members[index].type->GetPointerType(), loc);
		}
		else if (lhs.type->type == Types::Pointer && lhs.type->base->type == Types::Array && this->member.text.length() == 0)//or pointer!!(later)
		{
			std::vector<llvm::Value*> iindex = { context->root->builder.getInt32(0), context->DoCast(context->root->IntType, index->Compile(context)).val };

			auto loc = context->root->builder.CreateGEP(lhs.val, iindex, "index");

			return CValue(lhs.type/*->base*/, loc);
		}
		else if (lhs.type->type == Types::Pointer && lhs.type->base->type == Types::Pointer && this->member.text.length() == 0)//or pointer!!(later)
		{
			std::vector<llvm::Value*> iindex = { context->DoCast(context->root->IntType, index->Compile(context)).val };

			//loadme!!!
			lhs.val = context->root->builder.CreateLoad(lhs.val);
			//llllload my index
			auto loc = context->root->builder.CreateGEP(lhs.val, iindex, "index");

			return CValue(lhs.type->base, loc);
		}
		else if (lhs.type->type == Types::Struct && this->member.text.length() == 0)
		{
			context->root->Error("Indexing Structs Not Implemented", this->token);
		}
		context->root->Error("Cannot index type '" + lhs.type->ToString() + "'", this->token);
	}

	context->root->Error("Unimplemented", this->token);
}


void IndexExpression::CompileStore(CompilerContext* context, CValue right)
{
	context->CurrentToken(&token);
	context->SetDebugLocation(this->token);
	auto loc = this->GetElementPointer(context);

	right = context->DoCast(loc.type->base, right);
	context->root->builder.CreateStore(right.val, loc.val);
}

CValue StringExpression::Compile(CompilerContext* context)
{
	return context->String(this->value);
}

/*void NullExpression::Compile(CompilerContext* context)
{
context->Null();

//pop off if we dont need the result
if (dynamic_cast<BlockExpression*>(this->Parent))
context->Pop();
}*/

CValue NumberExpression::Compile(CompilerContext* context)
{
	bool isint = true;
	bool ishex = false;
	for (int i = 0; i < this->token.text.length(); i++)
	{
		if (this->token.text[i] == '.')
			isint = false;
	}

	if (token.text.length() >= 3)
	{
		std::string substr = token.text.substr(2);
		if (token.text[1] == 'x')
		{
			unsigned long long num = std::stoull(substr, nullptr, 16);
			return context->Integer(num);
		}
		else if (token.text[1] == 'b')
		{
			unsigned long long num = std::stoull(substr, nullptr, 2);
			return context->Integer(num);
		}
	}

	//ok, lets get the type from what kind of constant it is
	if (isint)
		return context->Integer(std::stoi(this->token.text));
	else
		return context->Float(::atof(token.text.c_str()));
}

CValue AssignExpression::Compile(CompilerContext* context)
{
	context->SetDebugLocation(this->token);

	if (auto storable = dynamic_cast<IStorableExpression*>(this->left))
		storable->CompileStore(context, right->Compile(context));

	return CValue();
}

CValue NameExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&token);

	return context->Load(token.text);
}

CValue OperatorAssignExpression::Compile(CompilerContext* context)
{
	//try and cast right side to left
	auto lhs = this->left->Compile(context);
	auto rhs = this->right->Compile(context);
	rhs = context->DoCast(lhs.type, rhs);

	context->CurrentToken(&token);
	context->SetDebugLocation(token);
	auto res = context->BinaryOperation(token.type, lhs, rhs);

	//insert store here
	if (auto storable = dynamic_cast<IStorableExpression*>(this->left))
		storable->CompileStore(context, res);

	return CValue();
}

CValue OperatorExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&this->_operator);
	context->SetDebugLocation(this->_operator);
	if (this->_operator.type == TokenType::And)
	{
		auto else_block = llvm::BasicBlock::Create(llvm::getGlobalContext(), "land.shortcircuitelse");
		auto end_block = llvm::BasicBlock::Create(llvm::getGlobalContext(), "land.endshortcircuit");
		auto cur_block = context->root->builder.GetInsertBlock();

		auto cond = this->left->Compile(context);
		cond = context->DoCast(context->root->BoolType, cond);
		context->root->builder.CreateCondBr(cond.val, else_block, end_block);

		context->function->f->getBasicBlockList().push_back(else_block);
		context->root->builder.SetInsertPoint(else_block);
		auto cond2 = this->right->Compile(context);

		cond2 = context->DoCast(context->root->BoolType, cond2);
		context->root->builder.CreateBr(end_block);

		context->function->f->getBasicBlockList().push_back(end_block);
		context->root->builder.SetInsertPoint(end_block);
		auto phi = context->root->builder.CreatePHI(cond.type->GetLLVMType(), 2, "land");
		phi->addIncoming(cond.val, cur_block);
		phi->addIncoming(cond2.val, else_block);

		return CValue(context->root->BoolType, phi);
	}

	if (this->_operator.type == TokenType::Or)
	{
		auto else_block = llvm::BasicBlock::Create(llvm::getGlobalContext(), "lor.shortcircuitelse");
		auto end_block = llvm::BasicBlock::Create(llvm::getGlobalContext(), "lor.endshortcircuit");
		auto cur_block = context->root->builder.GetInsertBlock();

		auto cond = this->left->Compile(context);
		cond = context->DoCast(context->root->BoolType, cond);
		context->root->builder.CreateCondBr(cond.val, end_block, else_block);

		context->function->f->getBasicBlockList().push_back(else_block);
		context->root->builder.SetInsertPoint(else_block);
		auto cond2 = this->right->Compile(context);
		cond2 = context->DoCast(context->root->BoolType, cond2);
		context->root->builder.CreateBr(end_block);

		context->function->f->getBasicBlockList().push_back(end_block);
		context->root->builder.SetInsertPoint(end_block);

		auto phi = context->root->builder.CreatePHI(cond.type->GetLLVMType(), 2, "lor");
		phi->addIncoming(cond.val, cur_block);
		phi->addIncoming(cond2.val, else_block);
		return CValue(context->root->BoolType, phi);
	}

	auto lhs = this->left->Compile(context);
	auto rhs = this->right->Compile(context);
	rhs = context->DoCast(lhs.type, rhs);

	return context->BinaryOperation(this->_operator.type, lhs, rhs);
}

CValue NewExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&this->token);

	auto ty = context->root->LookupType(type.text);
	auto size = context->GetSizeof(ty);
	auto arr_size = this->size ? this->size->Compile(context).val : 0;
	if (this->size)
	{
		size.val = context->root->builder.CreateMul(size.val, arr_size);
	}
	CValue val = context->Call("malloc", { size });

	auto ptr = context->DoCast(ty->GetPointerType(), val, true);

	//run constructors
	if (ty->type == Types::Struct)
	{
		Function* fun = 0;
		if (ty->data->template_base)
			fun = ty->GetMethod(ty->data->template_base->name, { ptr.type }, context);
		else
			fun = ty->GetMethod(ty->data->name, { ptr.type }, context);
		fun->Load(context->root);
		if (this->size == 0)
		{//just one element, construct it
			context->root->builder.CreateCall(fun->f, { ptr.val });
		}
		else
		{//construct each child element
			llvm::Value* counter = context->root->builder.CreateAlloca(context->root->IntType->GetLLVMType(), 0, "newcounter");
			context->root->builder.CreateStore(context->Integer(0).val, counter);

			auto start = llvm::BasicBlock::Create(context->root->context, "start", context->root->current_function->function->f);
			auto body = llvm::BasicBlock::Create(context->root->context, "body", context->root->current_function->function->f);
			auto end = llvm::BasicBlock::Create(context->root->context, "end", context->root->current_function->function->f);

			context->root->builder.CreateBr(start);
			context->root->builder.SetInsertPoint(start);
			auto cval = context->root->builder.CreateLoad(counter, "curcount");
			auto res = context->root->builder.CreateICmpUGE(cval, arr_size);
			context->root->builder.CreateCondBr(res, end, body);

			context->root->builder.SetInsertPoint(body);
			auto elementptr = context->root->builder.CreateGEP(ptr.val, { cval });
			context->root->builder.CreateCall(fun->f, { elementptr });

			auto inc = context->root->builder.CreateAdd(cval, context->Integer(1).val);
			context->root->builder.CreateStore(inc, counter);

			context->root->builder.CreateBr(start);

			context->root->builder.SetInsertPoint(end);
		}
	}

	return ptr;
}
