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
		auto current = this->Parent;
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
		} while (current = current->Parent);
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

Type* FunctionExpression::TypeCheck(CompilerContext* context)
{
	//add locals
	//need to change context
	CompilerContext* nc = new CompilerContext(context->root, context);// context->AddFunction(this->GetRealName(), ret, argsv, Struct.text.length() > 0 ? true : false, is_lambda);// , this->varargs);
	if (this->name.text.length() == 0)
		return 0;
	if (auto str = dynamic_cast<StructExpression*>(this->Parent))
	{
		auto typ = context->root->LookupType(str->GetName(), false)->GetPointerType()->GetPointerType();
		nc->TCRegisterLocal("this", typ);
	}
	else if (this->Struct.text.length())
	{
		auto typ = context->root->LookupType(this->Struct.text, false)->GetPointerType()->GetPointerType();
		nc->TCRegisterLocal("this", typ);

		return 0;
		//lets not typecheck these yet....
		if (typ->base->base->type == Types::Trait)
		{
			context->root->ns = typ->base->base->trait;
		}
		else
			context->root->ns = typ->base->base->data;
	}
	for (auto ii : *this->args)
		nc->TCRegisterLocal(ii.second, context->root->LookupType(ii.first)->GetPointerType());

	if (this->is_generator)
	{
		auto func = context->root->ns->GetFunction(this->GetRealName());
		auto str = func->return_type;
		nc->local_reg_callback = [&](const std::string& name, Type* ty)
		{
			str->data->struct_members.push_back({ name, ty->base->name, ty->base });
		};
	}
	this->block->TypeCheck(nc);

	if (this->Struct.text.length())
		context->root->ns = context->root->ns->parent;

	nc->local_reg_callback = [&](const std::string& name, Type* ty){};

	//add locals
	delete nc;

	return 0;
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
	{
		auto r = right->Compile(context);

		storable->CompileStore(context, r);
	}

	return CValue();
}

CValue CallExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&this->token);
	context->SetDebugLocation(this->token);

	std::vector<CValue> argsv;

	std::string fname;
	Type* stru = 0;
	if (auto name = dynamic_cast<NameExpression*>(left))
	{
		//ok handle what to do if im an index expression
		fname = name->GetName();

		//need to use the template stuff how to get it working with index expressions tho???
	}
	else if (auto index = dynamic_cast<IndexExpression*>(left))
	{
		//im a struct yo
		fname = index->member.text;
		stru = index->GetBaseType(context);
		assert(stru->loaded);
		llvm::Value* self = index->GetBaseElementPointer(context).val;
		if (index->token.type == TokenType::Pointy)
		{
			if (stru->type != Types::Pointer && stru->type != Types::Array)
				context->root->Error("Cannot dereference type " + stru->ToString(), this->token);

			stru = stru->base;
			self = context->root->builder.CreateLoad(self);
		}

		//push in the this pointer argument kay
		argsv.push_back(CValue(stru->GetPointerType(), self));
	}
	else
	{
		auto lhs = this->left->Compile(context);
		if (lhs.type->type != Types::Function)
			context->root->Error("Cannot call non-function", *context->current_token);

		std::vector<llvm::Value*> argts;
		for (auto ii : *this->args)
			argts.push_back(ii->Compile(context).val);
		return CValue(lhs.type->function->return_type, context->root->builder.CreateCall(lhs.val, argts));
	}

	//build arg list
	for (auto ii : *this->args)
		argsv.push_back(ii->Compile(context));

	context->CurrentToken(&this->token);
	return context->Call(fname, argsv, stru);
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

unsigned int uuid = 5;
std::string FunctionExpression::GetRealName()
{
	std::string fname;
	if (name.text.length() > 0)
		fname = name.text;
	else
		fname = "_lambda_id_" + std::to_string(uuid++);
	auto Struct = dynamic_cast<StructExpression*>(this->Parent);
	if (this->Struct.text.length() > 0)
		return "__" + this->Struct.text + "_" + fname;
	else
		return Struct ? "__" + Struct->GetName() + "_" + fname : fname;
}

CValue FunctionExpression::Compile(CompilerContext* context)
{
	auto Struct = dynamic_cast<StructExpression*>(this->Parent) ? dynamic_cast<StructExpression*>(this->Parent)->GetName() : this->Struct.text;

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

CValue FunctionExpression::DoCompile(CompilerContext* context)
{
	context->CurrentToken(&token);

	bool is_lambda = name.text.length() == 0;

	//build list of types of vars
	std::vector<std::pair<Type*, std::string>> argsv;
	auto Struct = dynamic_cast<StructExpression*>(this->Parent) ? dynamic_cast<StructExpression*>(this->Parent)->GetName() : this->Struct.text;

	//insert this argument if I am a member function
	if (Struct.length() > 0)
	{
		auto type = context->root->LookupType(Struct + "*");

		argsv.push_back({ type, "this" });
	}

	llvm::BasicBlock* yieldbb;//location of starting point in generator function
	if (this->is_generator)
	{
		//add the context pointer as an argument if this is a generator
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
		auto call = dynamic_cast<CallExpression*>(this->Parent);

		//find my type
		if (call)
		{
			//look for me in the args
			if (call->left == this)
				context->root->Error("Cannot imply type of lambda with the args of its call", *context->current_token);

			int i = dynamic_cast<NameExpression*>(call->left) ? 0 : 1;
			for (; i < call->args->size(); i++)
			{
				if ((*call->args)[i] == this)
					break;
			}
			CValue fun = call->left->Compile(context);
			Type* type = fun.type->function->args[i];

			if (type->type == Types::Function)
			{
				//do type inference
				int i2 = 0;
				for (auto& ii : *this->args)
				{
					if (ii.first.length() == 0)//do type inference
						ii.first = type->function->args[i2]->ToString();
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
					if (ii.first.length() == 0)//do type inference
						ii.first = fun->args[i2]->ToString();
					i2++;
				}

				if (this->ret_type.text.length() == 0)//infer return type
					this->ret_type.text = fun->return_type->ToString();
			}
		}
		else
		{
			for (auto ii : *this->args)
				if (ii.first.length() == 0)
					context->root->Error("Lambda type inference only implemented for function calls", *context->current_token);
			if (this->ret_type.text.length() == 0)
				context->root->Error("Lambda type inference only implemented for function calls", *context->current_token);
		}
	}

	for (auto ii : *this->args)
		argsv.push_back({ context->root->LookupType(ii.first), ii.second });

	context->CurrentToken(&this->ret_type);
	Type* ret;
	if (this->is_generator)
		ret = context->root->LookupType("bool");
	else
		ret = context->root->LookupType(this->ret_type.text);

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

		argsv.push_back({ context->root->LookupType("char*"), "_capture_data" });
	}

	CompilerContext* function;
	if (this->is_generator)
	{
		function = context->AddFunction(this->GetRealName() + "_generator", ret, { argsv.front() }, Struct.length() > 0 ? true : false, is_lambda);// , this->varargs);
		function->function->is_generator = true;
	}
	else
		function = context->AddFunction(this->GetRealName(), ret, argsv, Struct.length() > 0 ? true : false, is_lambda);// , this->varargs);

	function->function->storage_type = storage_t;
	context->root->current_function = function;
	function->function->Load(context->root);

	function->SetDebugLocation(this->token);

	if (is_lambda)
		function->function->do_export = false;

	//alloc args
	auto AI = function->function->f->arg_begin();
	for (unsigned Idx = 0, e = argsv.size(); Idx != e; ++Idx, ++AI)
	{
		// Create an alloca for this variable.
		if (!(Idx > 0 && this->is_generator))
		{
			auto aname = argsv[Idx].second;

			llvm::IRBuilder<> TmpB(&function->function->f->getEntryBlock(), function->function->f->getEntryBlock().begin());
			auto Alloca = TmpB.CreateAlloca(argsv[Idx].first->GetLLVMType(), 0, aname);
			// Store the initial value into the alloca.
			function->root->builder.CreateStore(AI, Alloca);

			AI->setName(aname);

			auto D = context->root->debug->createLocalVariable(llvm::dwarf::DW_TAG_arg_variable, function->function->scope, aname, context->root->debug_info.file, this->token.line,
				argsv[Idx].first->GetDebugType(context->root));

			llvm::Instruction *Call = context->root->debug->insertDeclare(
				Alloca, D, context->root->debug->createExpression(), llvm::DebugLoc::get(this->token.line, this->token.column, function->function->scope), Alloca);
			Call->setDebugLoc(llvm::DebugLoc::get(this->token.line, this->token.column, function->function->scope));

			// Add arguments to variable symbol table.
			function->RegisterLocal(aname, CValue(argsv[Idx].first->GetPointerType(), Alloca));
		}
	}

	if (this->is_generator)
	{
		//compile the start code for a generator function
		auto data = function->Load("_context");

		//add arguments
		int i = 0;
		for (auto ii : *this->args)
		{
			auto ptr = data.val;
			auto val = function->root->builder.CreateGEP(ptr, { function->root->builder.getInt32(0), function->root->builder.getInt32(2 + i++) });
			function->RegisterLocal(ii.second, CValue(function->root->LookupType(ii.first)->GetPointerType(), val));
		}

		//add local vars
		for (int i = 2 + this->args->size(); i < data.type->base->data->struct_members.size(); i++)
		{
			auto gep = context->root->builder.CreateGEP(data.val, { context->root->builder.getInt32(0), context->root->builder.getInt32(i) });
			function->function->variable_geps.push_back(gep);
		}

		//branch to the continue point
		auto br = function->root->builder.CreateGEP(data.val, { context->root->builder.getInt32(0), context->root->builder.getInt32(0) });
		auto loc = function->root->builder.CreateLoad(br);
		auto ibr = function->root->builder.CreateIndirectBr(loc, 10);
		function->function->ibr = ibr;

		//add new bb
		yieldbb = llvm::BasicBlock::Create(llvm::getGlobalContext(), "yield", function->function->f);
		ibr->addDestination(yieldbb);

		function->root->builder.SetInsertPoint(yieldbb);
	}

	block->Compile(function);

	//check for return, and insert one or error if there isnt one
	if (function->function->is_generator)
		function->root->builder.CreateRet(function->root->builder.getInt1(false));//signal we are gone generating values
	else if (function->function->f->getBasicBlockList().back().getTerminator() == 0)
		if (this->ret_type.text == "void")
			function->Return(CValue());
		else
			context->root->Error("Function must return a value!", token);

	if (this->is_generator)
	{
		//compile the other function necessary for an iterator
		auto func = context->AddFunction(this->GetRealName(), 0, { argsv.front() }, Struct.length() > 0 ? true : false, is_lambda);

		auto str = func->function->return_type;

		context->root->current_function = func;
		func->function->Load(context->root);

		//alloca the new context
		auto alloc = context->root->builder.CreateAlloca(str->GetLLVMType());

		//set the branch location to the start
		auto ptr = context->root->builder.CreateGEP(alloc, { context->root->builder.getInt32(0), context->root->builder.getInt32(0) });
		auto val = llvm::BlockAddress::get(yieldbb);
		context->root->builder.CreateStore(val, ptr);

		//store arguments into context
		if (argsv.size() > 1)
		{
			auto AI = func->function->f->arg_begin();
			for (int i = 1; i < argsv.size(); i++)
			{
				auto ptr = context->root->builder.CreateGEP(alloc, { context->root->builder.getInt32(0), context->root->builder.getInt32(2 + i - 1) });
				context->root->builder.CreateStore(AI++, ptr);
			}
		}

		//then return the newly created iterator object
		context->root->builder.CreateRet(context->root->builder.CreateLoad(alloc));

		//now compile reset function
		{
			auto reset = context->AddFunction(this->GetRealName() + "yield_reset", context->root->LookupType("void"), { argsv.front() }, Struct.length() > 0 ? true : false, is_lambda);// , this->varargs);

			context->root->current_function = reset;
			reset->function->Load(context->root);

			auto self = reset->function->f->arg_begin();
			//set the branch location back to start
			auto ptr = context->root->builder.CreateGEP(self, { context->root->builder.getInt32(0), context->root->builder.getInt32(0) });
			auto val = llvm::BlockAddress::get(yieldbb);
			context->root->builder.CreateStore(val, ptr);

			context->root->builder.CreateRetVoid();

			auto& x = str->data->functions.find("Reset");
			x->second = reset->function;
		}

		//compile current function
		{
			auto current = context->AddFunction(this->GetRealName() + "generator_current", context->root->LookupType(this->ret_type.text), { argsv.front() }, Struct.length() > 0 ? true : false, is_lambda);// , this->varargs);

			//add a return and shizzle
			context->root->current_function = current;
			current->function->Load(context->root);

			//return the current value
			auto self = current->function->f->arg_begin();
			auto ptr = context->root->builder.CreateGEP(self, { context->root->builder.getInt32(0), context->root->builder.getInt32(1) });
			context->root->builder.CreateRet(context->root->builder.CreateLoad(ptr));

			auto& x = str->data->functions.find("Current");
			x->second = current->function;
		}

		//Set the generator function as MoveNext
		auto& x = str->data->functions.find("MoveNext");
		x->second = function->function;
	}

	//reset insertion point to where it was before (for lambdas and template compilation)
	context->root->builder.SetCurrentDebugLocation(dp);
	if (rp)
		context->root->builder.SetInsertPoint(rp);
	context->root->current_function = context;

	//store the lambda value
	if (lambda)
	{
		function->WriteCaptures(lambda);

		auto ptr = context->root->builder.CreateGEP(lambda, { context->root->builder.getInt32(0), context->root->builder.getInt32(0) }, "name");

		ptr = context->root->builder.CreatePointerCast(ptr, function->function->f->getType()->getPointerTo());
		context->root->builder.CreateStore(function->function->f, ptr);
	}

	context->CurrentToken(&this->token);
	if (lambda)//return the lambda if we are one
		return CValue(lambda_type, context->root->builder.CreateLoad(lambda));
	else
		return CValue(function->function->GetType(context->root), function->function->f);
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
	context->root->functions.push_back(fun);
	auto str = dynamic_cast<StructExpression*>(this->Parent) ? dynamic_cast<StructExpression*>(this->Parent)->GetName() : this->Struct.text;

	fun->f = 0;
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

			//if (type->data->templates.size())
				//return;
		}
	}
	else
		context->root->ns->members.insert({ fname, fun });


	if (is_generator)
	{
		//build data about the generator context struct
		Type* str = new Type;
		str->name = this->GetRealName() + "_yielder_context";
		str->type = Types::Struct;
		str->data = new Jet::Struct;
		str->data->name = str->name;
		str->data->parent_struct = 0;
		context->root->ns->members.insert({ str->name, str });


		//add default iterator methods, will fill in function later
		str->data->functions.insert({ "MoveNext", 0 });
		str->data->functions.insert({ "Reset", 0 });
		str->data->functions.insert({ "Current", 0 });

		//add the position and return variables to the context
		str->data->struct_members.push_back({ "position", "char*", context->root->LookupType("char*") });
		str->data->struct_members.push_back({ "return", this->ret_type.text, context->root->LookupType(this->ret_type.text) });

		//add arguments to context
		for (auto ii : *this->args)
			str->data->struct_members.push_back({ ii.second, ii.first, context->root->LookupType(ii.first) });

		//str->data->struct_members.push_back
		//add any arguments, todo


		str->ns = context->root->ns;

		//DO NOT LOAD THIS UNTIL COMPILATION, IN FACT DONT LOAD ANYTHING
		//str->Load(context->root);
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
		Type* type = 0;
		if (!advlookup)//else
			type = context->root->LookupType(ii.first);

		fun->arguments.push_back({ type, ii.second });
		if (advlookup && is_trait == false)
			context->root->AdvanceTypeLookup(&fun->arguments.back().first, ii.first, &this->token);
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

	if (auto attr = dynamic_cast<AttributeExpression*>(this->Parent))
	{
		//add the attribute to the fun here
		if (attr->name.text == "stdcall")
			fun->calling_convention = CallingConvention::StdCall;
		//add the attribute to the fun
	}
	context->root->AdvanceTypeLookup(&fun->return_type, this->ret_type.text, &this->ret_type);

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
				context->root->Error("Cannot define a function for a type that is not a struct", token);

			ii->data->functions.insert({ fname, fun });
		}

		fun->arguments.push_back({ ii->GetPointerType(), "this" });
	}
	else
	{
		context->root->ns->members.insert({ fname, fun });
	}

	for (auto ii : *this->args)
	{
		fun->arguments.push_back({ 0, ii.second });
		context->root->AdvanceTypeLookup(&fun->arguments.back().first, ii.first, &this->token);
	}
}

CValue LocalExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&(*_names)[0].second);

	if (this->Parent->Parent == 0)
	{
		//im in global scope
		auto type = context->root->LookupType(this->_names->front().first.text);
		auto global = context->root->AddGlobal(this->_names->front().second.text, type);

		//should I add a constructor?
		if (this->_right && this->_right->size() > 0)
		{
			context->root->Error("Initializing global variables not yet implemented", token);
		}

		return CValue();
	}

	int i = 0;
	for (auto ii : *this->_names) {
		auto aname = ii.second.text;

		Type* type = 0;
		llvm::AllocaInst* Alloca = 0;
		if (ii.first.text.length() > 0)//type was specified
		{
			type = context->root->LookupType(ii.first.text);

			if (type->type == Types::Array)
				Alloca = context->root->builder.CreateAlloca(type->GetLLVMType(), context->root->builder.getInt32(type->size), aname);
			else
			{
				if (type->GetBaseType()->type == Types::Trait)
					context->root->Error("Cannot instantiate trait", ii.second);

				Alloca = context->root->builder.CreateAlloca(type->GetLLVMType(), 0, aname);
			}

			auto D = context->root->debug->createLocalVariable(llvm::dwarf::DW_TAG_auto_variable, context->function->scope, aname, context->root->debug_info.file, ii.second.line,
				type->GetDebugType(context->root));

			llvm::Instruction *Call = context->root->debug->insertDeclare(
				Alloca, D, context->root->debug->createExpression(), llvm::DebugLoc::get(this->token.line, this->token.column, context->function->scope), Alloca);
			Call->setDebugLoc(llvm::DebugLoc::get(ii.second.line, ii.second.column, context->function->scope));

			// Store the initial value into the alloca.
			if (this->_right)
			{
				auto val = (*this->_right)[i++]->Compile(context);
				//cast it
				val = context->DoCast(type, val);
				context->root->builder.CreateStore(val.val, Alloca);
			}
		}
		else if (this->_right)
		{
			//infer the type
			auto val = (*this->_right)[i++]->Compile(context);
			type = val.type;

			if (val.type->type == Types::Array)
				Alloca = context->root->builder.CreateAlloca(val.type->GetLLVMType(), context->root->builder.getInt32(val.type->size), aname);
			else
				Alloca = context->root->builder.CreateAlloca(val.type->GetLLVMType(), 0, aname);

			llvm::DIFile* unit = context->root->debug_info.file;
			type->Load(context->root);
			llvm::DILocalVariable* D = context->root->debug->createLocalVariable(llvm::dwarf::DW_TAG_arg_variable, context->function->scope, aname, unit, ii.second.line,
				type->GetDebugType(context->root));

			llvm::Instruction *Call = context->root->debug->insertDeclare(
				Alloca, D, context->root->debug->createExpression(), llvm::DebugLoc::get(this->token.line, this->token.column, context->function->scope), Alloca);
			Call->setDebugLoc(llvm::DebugLoc::get(ii.second.line, ii.second.column, context->function->scope));

			context->root->builder.CreateStore(val.val, Alloca);
		}
		else
			context->root->Error("Cannot infer type from nothing!", ii.second);

		// Add arguments to variable symbol table.
		if (context->function->is_generator)
		{
			//context->root->Error("Local variables inside of generators not yet implemented.", this->token);
			//find the already added type with the same name
			auto ty = context->function->arguments[0].first->base;
			/*std::vector<llvm::Type*> types;
			for (int i = 0; i < ty->data->type->getStructNumElements(); i++)
			types.push_back(((llvm::StructType*)ty->data->type)->getElementType(i));
			((llvm::StructType*)ty->data->type)->setBody(types);
			ty->data->struct_members.push_back({ aname, "type", type });*/
			//ok, almost this doesnt quite work right
			auto var_ptr = context->function->variable_geps[context->function->var_num++];
			//var_ptr->dump();

			if (this->_right)
			{
				auto val = (*this->_right)[i - 1]->Compile(context);
				val = context->DoCast(type, val);

				context->root->builder.CreateStore(val.val, var_ptr);
			}

			//still need to do store
			context->RegisterLocal(aname, CValue(type->GetPointerType(), var_ptr));
		}
		else
			context->RegisterLocal(aname, CValue(type->GetPointerType(), Alloca));

		//construct it!
		if (this->_right == 0 && type->type == Types::Struct)
		{
			//call default construct if it exists
			const std::string& constructor_name = type->data->template_base ? type->data->template_base->name : type->data->name;
			auto iter = type->data->functions.find(constructor_name);
			if (iter != type->data->functions.end())
				context->Call(constructor_name, { CValue(type->GetPointerType(), Alloca) }, type);
		}
	}

	return CValue();
}

CValue StructExpression::Compile(CompilerContext* context)
{
	if (this->templates)
	{
		//dont compile, just verify that all traits are valid
		for (auto ii : *this->templates)
		{
			auto iter = context->root->TryLookupType(ii.first.text);
			if (iter == 0 || iter->type != Types::Trait)
				context->root->Error("Trait '" + ii.first.text + "' is not defined", ii.first);
		}

		//auto myself = context->root->LookupType(this->name.text, false);

		/*for (auto ii : *this->templates)
		{
		Type* type = context->root->LookupType(ii.first.text);

		myself->data->members.insert({ ii.second.text, type });
		}

		auto oldns = context->root->ns;
		context->root->ns = myself->data;
		myself->data->parent = oldns;
		for (auto& ii : myself->data->struct_members)
		{
		if (ii.type == 0)
		ii.type = context->root->LookupType(ii.type_name);
		}
		context->root->ns = oldns;
		//add in the type and tell it to not actually generate IR, just to check things
		for (auto ii : this->members)
		{
			//finish this
			if (ii.type == StructMember::FunctionMember)
				ii.function->Compile(context);
		}*/

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
	if (has_constructor == false)
	{
		auto fun = new Function("__" + str->data->name + "_" + strname, false);//
		fun->return_type = &VoidType;
		fun->arguments = { { context->root->LookupType(str->data->name + "*", false), "this" } };
		//fun->arguments.push_back({ 0, "this" });// = { { context->root->AdvanceTypeLookup(str->data->name + "*"), "this" } };
		//context->root->AdvanceTypeLookup(&fun->arguments.back().first, str->data->name + "*");
		fun->f = 0;
		fun->expression = 0;
		fun->type = (FunctionType*)-1;
		str->data->functions.insert({ strname, fun });//register function in _Struct
	}
	if (has_destructor == false)
	{
		auto fun = new Function("__" + str->data->name + "_~" + strname, false);//
		fun->return_type = &VoidType;
		fun->arguments = { { context->root->LookupType(str->data->name + "*", false), "this" } };
		//fun->arguments.push_back({ 0, "this" });// = { { context->root->AdvanceTypeLookup(str->data->name + "*"), "this" } };
		//context->root->AdvanceTypeLookup(&fun->arguments.back().first, str->data->name + "*");
		//fun->arguments = { { context->root->AdvanceTypeLookup(str->data->name + "*"), "this" } };
		fun->f = 0;
		fun->expression = 0;
		fun->type = (FunctionType*)-1;

		str->data->functions.insert({ "~" + strname, fun });
	}
}

void StructExpression::AddConstructors(CompilerContext* context)
{
	auto Struct = this->GetName();

	Type* str = context->root->LookupType(Struct);//todo get me
	std::string strname = str->data->template_base ? str->data->template_base->name : str->data->name;

	for (auto ii : str->data->functions)
	{
		//need to identify if its a autogenerated fun
		if (ii.second->expression == 0 && ii.second->type == (FunctionType*)-1)//ii.second->f == 0)
		{
			//its probably a constructor/destructor we need to fill in
			auto res = ii.second->name.find('~');
			if (res != -1)
			{
				//destructor
				auto rp = context->root->builder.GetInsertBlock();
				auto dp = context->root->builder.getCurrentDebugLocation();

				std::vector<std::pair<Type*, std::string>> argsv;
				argsv.push_back({ context->root->LookupType(str->data->name + "*"), "this" });

				//context->CurrentToken(&this->ret_type);
				auto ret = context->root->LookupType("void");

				CompilerContext* function = context->AddFunction(ii.second->name, ret, argsv, Struct.length() > 0 ? true : false, false);// , this->varargs);
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

					auto D = context->root->debug->createLocalVariable(llvm::dwarf::DW_TAG_arg_variable, function->function->scope, aname, context->root->debug_info.file, this->token.line,
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
						//call the constructor
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
				argsv.push_back({ context->root->LookupType(str->data->name + "*"), "this" });

				//context->CurrentToken(&this->ret_type);
				auto ret = context->root->LookupType("void");

				CompilerContext* function = context->AddFunction(ii.second->name, ret, argsv, Struct.length() > 0 ? true : false, false);// , this->varargs);
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

					auto D = context->root->debug->createLocalVariable(llvm::dwarf::DW_TAG_arg_variable, function->function->scope, aname, unit, this->token.line,
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

			auto iter = ii.second->f->getBasicBlockList().begin()->begin();
			for (int i = 0; i < ii.second->arguments.size() * 3; i++)
				iter++;
			context->root->builder.SetInsertPoint(iter);

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
			for (int i = 0; i < ii.second->arguments.size() * 3; i++)
				iter++;
			context->root->builder.SetInsertPoint(iter);

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
	Type* str = new Type;
	context->root->ns->members.insert({ this->name.text, str });
	str->name = this->name.text;
	str->type = Types::Struct;
	str->data = new Struct;
	str->data->name = this->name.text;
	str->data->expression = this;
	if (this->base_type.text.length())
		context->root->AdvanceTypeLookup(&str->data->parent_struct, this->base_type.text, &this->base_type);
	if (this->templates)
	{
		str->data->templates.reserve(this->templates->size());
		for (auto& ii : *this->templates)
		{
			str->data->templates.push_back({ 0, ii.second.text });
			context->root->AdvanceTypeLookup(&str->data->templates.back().first, ii.first.text, &ii.first);

			context->root->AdvanceTypeLookup(&str->data->members.insert({ ii.second.text, Symbol((Type*)0) })->second.ty, ii.first.text, &ii.first);
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
			str->data->struct_members.push_back({ ii.variable.second.text, ii.variable.first.text, 0 });
			if (this->templates == 0)
				context->root->AdvanceTypeLookup(&str->data->struct_members.back().type, ii.variable.first.text, &ii.variable.first);
		}
		else
		{
			if (this->templates == 0)//todo need to get rid of this to fix things
				ii.function->CompileDeclarations(context);
		}
	}
	context->root->ns = context->root->ns->parent;

	if (this->templates == 0)
		this->AddConstructorDeclarations(str, context);
}

CValue DefaultExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&token);

	SwitchExpression* sw = dynamic_cast<SwitchExpression*>(this->Parent->Parent);
	if (sw == 0)
		context->root->Error("Cannot use default expression outside of a switch!", token);

	//create a new block for this case
	llvm::BasicBlock* block = sw->def;

	//add the case to the switch
	bool is_first = sw->first_case;
	sw->first_case = false;

	//jump to end block
	if (!is_first)
		context->root->builder.CreateBr(sw->switch_end);

	//start using our new block
	context->function->f->getBasicBlockList().push_back(block);
	context->root->builder.SetInsertPoint(block);
	return CValue();
}

CValue CaseExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&token);

	SwitchExpression* sw = dynamic_cast<SwitchExpression*>(this->Parent->Parent);
	if (sw == 0)
		context->root->Error("Cannot use case expression outside of a switch!", token);

	//create a new block for this case
	llvm::BasicBlock* block = llvm::BasicBlock::Create(llvm::getGlobalContext(), "case" + std::to_string(value));

	//add the case to the switch
	bool is_first = sw->AddCase(context->root->builder.getInt32(this->value), block);

	//jump to end block
	if (!is_first)
		context->root->builder.CreateBr(sw->switch_end);

	//start using our new block
	context->function->f->getBasicBlockList().push_back(block);
	context->root->builder.SetInsertPoint(block);
	return CValue();
}

CValue SwitchExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&token);

	CValue value = this->var->Compile(context);
	if (value.type->type != Types::Int)
		context->root->Error("Argument to Case Statement Must Be an Integer", token);

	this->switch_end = llvm::BasicBlock::Create(llvm::getGlobalContext(), "switchend");

	//look for all case and default expressions
	std::vector < CaseExpression* > cases;
	for (auto expr : this->block->statements)
	{
		auto Case = dynamic_cast<CaseExpression*>(expr);
		if (Case)
			cases.push_back(Case);

		//add default parser and expression
		else if (auto def = dynamic_cast<DefaultExpression*>(expr))
		{
			//do default
			if (this->def)
				context->root->Error("Multiple defaults defined for the same switch!", token);
			this->def = llvm::BasicBlock::Create(llvm::getGlobalContext(), "switchdefault");
		}
	}

	bool no_def = def ? false : true;
	if (def == 0)
	{
		//create default block at end if there isnt one
		this->def = llvm::BasicBlock::Create(llvm::getGlobalContext(), "switchdefault");
	}

	//create the switch instruction
	this->sw = context->root->builder.CreateSwitch(value.val, def, cases.size());

	//compile the block
	this->block->Compile(context);

	context->root->builder.CreateBr(this->switch_end);

	if (no_def)
	{
		//insert and create a dummy default
		context->function->f->getBasicBlockList().push_back(def);
		context->root->builder.SetInsertPoint(def);
		context->root->builder.CreateBr(this->switch_end);
	}

	//start using end
	context->function->f->getBasicBlockList().push_back(this->switch_end);
	context->root->builder.SetInsertPoint(this->switch_end);

	return CValue();
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
	//else if (tr->second->valid == false)//make sure it is set as invalid
	//t = tr->second;
	else
		context->root->Error("Type '" + name.text + "' already exists", token);
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
			if (ii.first.text.length() == 0)
				t->templates.push_back({ 0, ii.second.text });
			else
			{
				t->templates.push_back({ 0, ii.second.text });
				context->root->AdvanceTypeLookup(&t->templates.back().first, ii.first.text, &ii.first);
			}

			auto type = new Type;
			type->name = ii.second.text;
			type->type = Types::Invalid;
			type->ns = context->root->ns;
			t->members.insert({ ii.second.text, type });
		}
	}

	for (auto ii : this->funcs)
	{
		Function* func = new Function("", false);
		/*func->return_type = */context->root->AdvanceTypeLookup(&func->return_type, ii.ret_type.text, &ii.ret_type);
		func->arguments.reserve(ii.args.size());
		for (auto arg : ii.args)
		{
			func->arguments.push_back({ 0, "dummy" });
			context->root->AdvanceTypeLookup(&func->arguments.back().first, arg.text, &arg);
		}

		t->functions.insert({ ii.name.text, func });
	}

	context->root->ns = t->parent;
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

Type* CallExpression::TypeCheck(CompilerContext* context)
{
	Type* stru = 0;
	std::string fname;
	if (auto name = dynamic_cast<NameExpression*>(left))
	{
		//ok handle what to do if im an index expression
		fname = name->GetName();

		//need to use the template stuff how to get it working with index expressions tho???
	}
	else if (auto index = dynamic_cast<IndexExpression*>(left))
	{
		//im a struct yo
		fname = index->member.text;
		stru = index->GetBaseType(context, true);
		//stru->Load(context->root);
		//assert(stru->loaded);
		//llvm::Value* self = index->GetBaseElementPointer(context).val;
		if (index->token.type == TokenType::Pointy)
		{
			if (stru->type != Types::Pointer && stru->type != Types::Array)
				context->root->Error("Cannot dereference type " + stru->ToString(), this->token);

			stru = stru->base;
			//self = context->root->builder.CreateLoad(self);
		}

		//push in the this pointer argument kay
		//argsv.push_back(CValue(stru->GetPointerType(), self));
	}
	else
	{
		throw 7;
		/*auto lhs = this->left->Compile(context);
		if (lhs.type->type != Types::Function)
		context->root->Error("Cannot call non-function", *context->current_token);

		std::vector<llvm::Value*> argts;
		for (auto ii : *this->args)
		{
		auto val = ii->Compile(context);
		argts.push_back(val.val);
		}
		return lhs.type->function;*/// CValue(lhs.type->function->return_type, context->root->builder.CreateCall(lhs.val, argts));
	}
	std::vector<Type*> arg;
	if (stru)
		arg.push_back(stru->GetPointerType());

	for (auto ii : *args)
	{
		arg.push_back(ii->TypeCheck(context));
	}
	auto fun = context->GetMethod(fname, arg, stru);
	if (fun == 0)
	{
		//check variables
		auto var = context->TCGetVariable(fname);
		if (var->type == Types::Pointer && var->base->type == Types::Struct && var->base->data->template_base && var->base->data->template_base->name == "function")
			return var->base->data->members.find("T")->second.ty->function->return_type;
		else if (var->base->type == Types::Function)
			return var->base->function->return_type;

		context->root->Error("Cannot call method '" + fname + "'", this->token);
	}

	if (fun->arguments.size() == arg.size() + 1)
	{
		//its a constructor or something
		return fun->arguments[0].first->base;
		//throw 7;
	}

	//keep working on this, dont forget constructors
	//auto left = this->left->TypeCheck(context);
	//todo: check args


	//throw 7;
	return fun->return_type;
}

CValue YieldExpression::Compile(CompilerContext* context)
{
	//first make sure we are in a generator...
	if (context->function->is_generator == false)
		context->root->Error("Cannot use yield outside of a generator!", this->token);

	//create a new block for after the yield
	auto bb = llvm::BasicBlock::Create(llvm::getGlobalContext(), "yield");
	context->function->f->getBasicBlockList().push_back(bb);

	//add the new block to the indirect branch list
	context->function->ibr->addDestination(bb);

	//store the current location into the generator context
	auto data = context->Load("_context");
	auto br = context->root->builder.CreateGEP(data.val, { context->root->builder.getInt32(0), context->root->builder.getInt32(0) });
	auto ba = llvm::BlockAddress::get(bb);
	context->root->builder.CreateStore(ba, br);

	if (this->right)
	{
		//compile the yielded value
		auto value = right->Compile(context);

		//store result into the generator context
		value = context->DoCast(data.type->base->data->struct_members[1].type, value);//cast to the correct type
		br = context->root->builder.CreateGEP(data.val, { context->root->builder.getInt32(0), context->root->builder.getInt32(1) });
		context->root->builder.CreateStore(value.val, br);
	}

	//return 1 to say we are not finished yielding
	context->root->builder.CreateRet(context->root->builder.getInt1(true));

	//start inserting in new block
	context->root->builder.SetInsertPoint(bb);

	return CValue();
}

CValue MatchExpression::Compile(CompilerContext* context)
{
	CValue val;//first get pointer to union
	auto i = dynamic_cast<NameExpression*>(var);
	auto p = dynamic_cast<IndexExpression*>(var);
	if (i)
		val = context->GetVariable(i->GetName());
	else if (p)
		val = p->GetElementPointer(context);

	if (val.type->base->type != Types::Union)
		context->root->Error("Cannot match with a non-union", token);

	auto endbb = llvm::BasicBlock::Create(llvm::getGlobalContext(), "match.end");

	//from val get the type
	auto key = context->root->builder.CreateGEP(val.val, { context->root->builder.getInt32(0), context->root->builder.getInt32(0) });
	auto sw = context->root->builder.CreateSwitch(context->root->builder.CreateLoad(key), endbb, this->cases.size());

	for (auto ii : this->cases)
	{
		context->PushScope();

		if (ii.type.type == TokenType::Default)
		{
			//add bb for case
			auto bb = llvm::BasicBlock::Create(llvm::getGlobalContext(), "match.case", context->function->f);
			context->root->builder.SetInsertPoint(bb);
			sw->setDefaultDest(bb);

			//build internal
			ii.block->Compile(context);

			//branch to end
			context->root->builder.CreateBr(endbb);
			break;
		}

		int pi = 0;//find what index it is
		for (auto mem : val.type->base->_union->members)
		{
			if (mem->name == ii.type.text)
				break;
			pi++;
		}

		if (pi >= val.type->base->_union->members.size())
			context->root->Error("Type '" + ii.type.text + "' not in union, cannot match to it.", ii.type);

		//add bb for case
		auto bb = llvm::BasicBlock::Create(llvm::getGlobalContext(), "match.case", context->function->f);
		context->root->builder.SetInsertPoint(bb);
		auto i = llvm::ConstantInt::get(llvm::getGlobalContext(), llvm::APInt(32, (uint64_t)pi));
		sw->addCase(i, bb);

		//add local
		auto ptr = context->root->builder.CreateGEP(val.val, { context->root->builder.getInt32(0), context->root->builder.getInt32(1) });
		ptr = context->root->builder.CreatePointerCast(ptr, val.type->base->_union->members[pi]->GetPointerType()->GetLLVMType());
		context->RegisterLocal(ii.name.text, CValue(val.type->base->_union->members[pi]->GetPointerType(), ptr));

		//build internal
		ii.block->Compile(context);

		//need to do this without destructing args
		context->scope->named_values[ii.name.text] = CValue();
		context->PopScope();

		//branch to end
		context->root->builder.CreateBr(endbb);
	}

	//start new basic block
	context->function->f->getBasicBlockList().push_back(endbb);
	context->root->builder.SetInsertPoint(endbb);

	return CValue();
}
