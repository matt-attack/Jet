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

CValue GetPtrToExprValue(CompilerContext* context, Expression* right)
{
	auto i = dynamic_cast<NameExpression*>(right);
	auto p = dynamic_cast<IndexExpression*>(right);
	if (i)
	{
		return context->GetVariable(i->GetName());
	}
	else if (p)
	{
		return p->GetElementPointer(context);
	}
	context->root->Error("Not Implemented", *context->current_token);
}

CValue PrefixExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&this->_operator);

	if (this->_operator.type == TokenType::BAnd)
		return GetPtrToExprValue(context, right);

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

CValue TypeofExpression::Compile(CompilerContext* context)
{
	return CValue(this->arg->TypeCheck(context), 0);
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
	return CValue(loc.type->base, context->root->builder.CreateLoad(loc.val), loc.val);
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

CValue IndexExpression::GetBaseElementPointer(CompilerContext* context)
{
	if (auto name = dynamic_cast<NameExpression*>(left))
		return context->GetVariable(name->GetName());
	else if (auto index = dynamic_cast<IndexExpression*>(left))
		return index->GetElementPointer(context);
	else if (auto call = dynamic_cast<CallExpression*>(left))
	{
		//store the value then return the pointer to it
		auto val = call->Compile(context);
		auto alloca = context->root->builder.CreateAlloca(val.type->GetLLVMType());

		return CValue(val.type->GetPointerType(), alloca);
	}
	context->root->Error("Could not handle Get Base Element Pointer", token);
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
			context->root->Error("Struct Member '" + name + "' of Struct '" + type->data->name + "' Not Found", token);
		return method->GetType(context->root);
	}

	if (index >= type->data->struct_members.size())
		context->root->Error("Struct Member '" + name + "' of Struct '" + type->data->name + "' Not Found", token);

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
		CValue lhs;
		if (p)
			if (tc)
				lhs.type = context->TCGetVariable(p->GetName());
			else
				lhs = context->GetVariable(p->GetName());
		else if (i)
			lhs.type = i->GetType(context, tc)->GetPointerType();

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
			if (funiter != stru->functions.end() && funiter->second->arguments.size() == 2)
			{
				Function* fun = funiter->second;
				//CValue right = index->Compile(context);
				//todo: casting
				return fun->return_type->base;
			}
		}
	}
	throw 7;//error if we miss it
}

CValue GetStructElement(CompilerContext* context, const std::string& name, const Token& token, Type* type, llvm::Value* lhs)
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
			context->root->Error("Struct Member '" + name + "' of Struct '" + type->data->name + "' Not Found", token);
		method->Load(context->root);
		return CValue(method->GetType(context->root), method->f);
	}
	std::vector<llvm::Value*> iindex = { context->root->builder.getInt32(0), context->root->builder.getInt32(index) };

	auto loc = context->root->builder.CreateGEP(lhs, iindex, "index");
	return CValue(type->data->struct_members[index].type->GetPointerType(), loc);
}

CValue IndexExpression::GetElementPointer(CompilerContext* context)
{
	auto p = dynamic_cast<NameExpression*>(left);
	auto i = dynamic_cast<IndexExpression*>(left);

	CValue lhs;
	if (p)
		lhs = context->GetVariable(p->GetName());
	else if (i)
		lhs = i->GetElementPointer(context);

	if (index == 0 && this->token.type == TokenType::Pointy)
	{
		if (lhs.type->type == Types::Pointer && lhs.type->base->type == Types::Pointer && lhs.type->base->base->type == Types::Struct)
		{
			lhs.val = context->root->builder.CreateLoad(lhs.val);

			return GetStructElement(context, this->member.text, this->member, lhs.type->base->base, lhs.val);
		}

		//todo: improve these error messages
		context->root->Error("Cannot index type '" + lhs.type->base->name + "'", this->token);
	}
	else if (p || i)
	{
		if (this->member.text.length() && lhs.type->type == Types::Pointer && lhs.type->base->type == Types::Struct)
		{
			return GetStructElement(context, this->member.text, this->member, lhs.type->base, lhs.val);
		}
		else if (lhs.type->type == Types::Pointer && lhs.type->base->type == Types::Array && this->member.text.length() == 0)//or pointer!!(later)
		{
			std::vector<llvm::Value*> iindex = { context->root->builder.getInt32(0), context->DoCast(context->root->IntType, index->Compile(context)).val };
			auto first_loc = context->root->builder.CreateGEP(lhs.val, { context->root->builder.getInt32(0), context->root->builder.getInt32(1) });
			first_loc = context->root->builder.CreateLoad(first_loc);
			auto loc = context->root->builder.CreateGEP(first_loc/*lhs.val*/, context->DoCast(context->root->IntType, index->Compile(context)).val/*iindex*/, "index");
			auto typ = lhs.type->base->base->GetPointerType();
			return CValue(typ, loc);
		}
		else if (lhs.type->type == Types::Pointer && lhs.type->base->type == Types::Array)
		{
			if (this->member.text == "size")
			{
				auto loc = context->root->builder.CreateGEP(lhs.val, { context->root->builder.getInt32(0), context->root->builder.getInt32(0) });
				return CValue(context->root->IntType->GetPointerType(), loc);
			}
			else if (this->member.text == "ptr")
			{
				auto loc = context->root->builder.CreateGEP(lhs.val, { context->root->builder.getInt32(0), context->root->builder.getInt32(1) });
				return CValue(lhs.type->base->base->GetPointerType()->GetPointerType(), loc);
			}
		}
		else if (lhs.type->type == Types::Pointer && lhs.type->base->type == Types::Pointer && this->member.text.length() == 0)//or pointer!!(later)
		{
			//throw 7;
			std::vector<llvm::Value*> iindex = { context->DoCast(context->root->IntType, index->Compile(context)).val };

			//loadme!!!
			lhs.val = context->root->builder.CreateLoad(lhs.val);
			//llllload my index
			auto loc = context->root->builder.CreateGEP(lhs.val, iindex, "index");

			return CValue(lhs.type->base, loc);
		}
		//maybe this case shouldnt happen
		/*else if (lhs.type->type == Types::InternalArray && this->member.text.length() == 0)//or pointer!!(later)
		{
			//throw 7;
			std::vector<llvm::Value*> iindex = { context->root->builder.getInt32(0), context->DoCast(context->root->IntType, index->Compile(context)).val };

			//loadme!!!
			//lhs.val = context->root->builder.CreateLoad(lhs.val);
			//llllload my index
			auto loc = context->root->builder.CreateGEP(lhs.val, iindex, "index");
			
			return CValue(lhs.type->base->GetPointerType(), loc);
		}*/
		else if (lhs.type->type == Types::Pointer && lhs.type->base->type == Types::InternalArray && this->member.text.length() == 0)//or pointer!!(later)
		{
			//throw 7;
			std::vector<llvm::Value*> iindex = { context->root->builder.getInt32(0), context->DoCast(context->root->IntType, index->Compile(context)).val };

			//loadme!!!
			//lhs.val = context->root->builder.CreateLoad(lhs.val);
			//llllload my index
			auto loc = context->root->builder.CreateGEP(lhs.val, iindex, "index");

			return CValue(lhs.type->base->base->GetPointerType(), loc);
		}
		else if (lhs.type->type == Types::Pointer && lhs.type->base->type == Types::Struct && this->member.text.length() == 0)
		{
			auto stru = lhs.type->base->data;

			auto funiter = stru->functions.find("[]");
			//todo: search through multimap to find one with the right number of args
			if (funiter != stru->functions.end() && funiter->second->arguments.size() == 2)
			{
				Function* fun = funiter->second;
				fun->Load(context->root);
				CValue right = index->Compile(context);
				//todo: casting
				return CValue(fun->return_type, context->root->builder.CreateCall(fun->f, { lhs.val, right.val }, "operator_overload"));
			}
		}
		context->root->Error("Cannot index type '" + lhs.type->ToString() + "'", this->token);
	}

	context->root->Error("Unimplemented", this->token);
}


void IndexExpression::CompileStore(CompilerContext* context, CValue right)
{
	auto oldtok = context->current_token;
	context->CurrentToken(&token);
	context->SetDebugLocation(this->token);
	//todo: do not allow store into .size on arrays
	auto loc = this->GetElementPointer(context);

	context->CurrentToken(oldtok);

	context->Store(loc, right);

	/*right = context->DoCast(loc.type->base, right);

	if (loc.type->base->type == Types::Struct)
	{
		auto funiter = left.type->data->functions.find("=");
		//todo: search through multimap to find one with the right number of args
		if (funiter != left.type->data->functions.end() && funiter->second->arguments.size() == 2)
		{
			Function* fun = funiter->second;
			fun->Load(context->root);
			std::vector<llvm::Value*> argsv = { lhsptr.val, right.val };
			return CallFunction(this, fun, argsv, false);
			//ok, need to use proper function call here to include virtuals
			//return CValue(fun->return_type, this->root->builder.CreateCall(fun->f, { lhsptr.val, right.val }, "operator_overload"));
		}
	}
	context->root->builder.CreateStore(right.val, loc.val);*/
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

	context->CurrentToken(&this->token);

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
	context->CurrentToken(&token);
	context->SetDebugLocation(token);

	//try and cast right side to left
	auto lhs = this->left->Compile(context);
	CValue lhsptr;
	if (lhs.type->type == Types::Struct)
		lhsptr = GetPtrToExprValue(context, left);
	auto rhs = this->right->Compile(context);

	auto res = context->BinaryOperation(token.type, lhs, lhsptr, rhs);

	//insert store here
	if (auto storable = dynamic_cast<IStorableExpression*>(this->left))
		storable->CompileStore(context, res);
	else
		context->root->Error("Cannot store into this type.", this->token);
	return CValue();
}

CValue OperatorExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&this->_operator);
	context->SetDebugLocation(this->_operator);
	if (this->_operator.type == TokenType::And)
	{
		auto else_block = llvm::BasicBlock::Create(context->context, "land.shortcircuitelse");
		auto end_block = llvm::BasicBlock::Create(context->context, "land.endshortcircuit");
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
		auto else_block = llvm::BasicBlock::Create(context->context, "lor.shortcircuitelse");
		auto end_block = llvm::BasicBlock::Create(context->context, "lor.endshortcircuit");
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
	CValue lhsptr;
	if (lhs.type->type == Types::Struct)
		lhsptr = GetPtrToExprValue(context, left);

	auto rhs = this->right->Compile(context);

	return context->BinaryOperation(this->_operator.type, lhs, lhsptr, rhs);
}

CValue NewExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&this->token);

	auto ty = context->root->LookupType(type.text);
	auto size = context->GetSizeof(ty);
	auto arr_size = this->size ? this->size->Compile(context).val : context->root->builder.getInt32(1);
	
	//todo, on free mark size as zero
	if (this->size)
		size.val = context->root->builder.CreateMul(size.val, arr_size);
	size.val = context->root->builder.CreateAdd(size.val, context->root->builder.getInt32(4));
	
	//ok now get new working with it
	CValue val = context->Call("malloc", { size });

	auto pointer = context->root->builder.CreatePointerCast(val.val, context->root->IntType->GetPointerType()->GetLLVMType());
	context->root->builder.CreateStore(arr_size, pointer);
	val.val = context->root->builder.CreateGEP(val.val, { context->root->builder.getInt32(4) });

	//ok when we free this mark the size as zero and lets check size before we free
	//	this will make double frees IMPOSSIBLE on arrays :D
	//if we are in debug mark as zero and we can crash if we double free maybe?


	auto ptr = context->DoCast(ty->GetPointerType(), val, true);

	//run constructors
	if (this->args)
	{
		if (ptr.type->base->type == Types::Struct)// && arr_size == 0)
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
				fun = ty->GetMethod(ty->data->template_base->name, arg_types, context);
			else
				fun = ty->GetMethod(ty->data->name, arg_types, context);

			if (fun == 0)
			{
				std::string err = "Constructor for '";
				err += ty->data->template_base ? ty->data->template_base->name : ty->data->name;
				err += "' with arguments (";
				int i = 0;
				for (auto ii : arg_types)
				{
					err += ii->ToString();
					if (i++ < (arg_types.size() - 1))
						err += ',';
				}
				err += ") not found!";
				context->root->Error(err, this->token);
			}
			fun->Load(context->root);
			context->root->builder.CreateCall(fun->f, llvm_args);
		}
		else
			context->root->Error("Cannot construct non-struct type!", this->token);

		//handle constructor args
		return ptr;
	}
	context->Construct(ptr, arr_size);


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

		return CValue(str_type, str);
	}
	return ptr;
}

CValue FreeExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&this->token);

	auto pointer = this->pointer->Compile(context);

	if (pointer.type->type != Types::Pointer && pointer.type->type != Types::Array)
		context->root->Error("Cannot free a non pointer/array type!", this->token);

	//extract the pointer from the array if we are one
	if (pointer.type->type == Types::Array)
	{
		auto ptr = context->root->builder.CreateExtractValue(pointer.val, 1);
		pointer = CValue(pointer.type->base->GetPointerType(), ptr);	
	}

	//get to the root of the pointer (remove the offset for the size)
	llvm::Value* charptr = context->root->builder.CreatePointerCast(pointer.val, context->root->builder.getInt8PtrTy());
	llvm::Value* rootptr = context->root->builder.CreateGEP(charptr, { context->root->builder.getInt32(-4) });

	//run destructors
	if (pointer.type->base->type == Types::Struct)
	{
		Type* ty = pointer.type->base;
		Function* fun = 0;
		if (ty->data->template_base)
			fun = ty->GetMethod("~" + ty->data->template_base->name, { pointer.type }, context);
		else
			fun = ty->GetMethod("~" + ty->data->name, { pointer.type }, context);

		if (fun)
		{
			fun->Load(context->root);
			if (false)//this->close_bracket.text.length() == 0)//size == 0)
			{//just one element, destruct it
				rootptr = context->root->builder.CreatePointerCast(pointer.val, context->root->builder.getInt8PtrTy());

				context->root->builder.CreateCall(fun->f, { pointer.val });
			}
			else
			{
				auto arr_size = context->root->builder.CreateLoad(context->root->builder.CreatePointerCast(rootptr, context->root->IntType->GetPointerType()->GetLLVMType()));

				//destruct each child element
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
				auto elementptr = context->root->builder.CreateGEP(pointer.val, { cval });
				context->root->builder.CreateCall(fun->f, { elementptr });

				auto inc = context->root->builder.CreateAdd(cval, context->Integer(1).val);
				context->root->builder.CreateStore(inc, counter);

				context->root->builder.CreateBr(start);

				context->root->builder.SetInsertPoint(end);
			}
		}
	}

	context->Call("free", { CValue(context->root->CharPointerType/*LookupType("char*")*/, rootptr) });

	//todo: can mark the size and pointer as zero now

	return CValue();
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
			context->root->Error("Invalid Unary Operation '" + TokenToString[this->_operator.type] + "' On Type '" + type->ToString() + "'", *context->current_token);
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
			context->root->Error("Invalid Unary Operation '" + TokenToString[this->_operator.type] + "' On Type '" + type->ToString() + "'", *context->current_token);
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
	context->root->Error("Invalid Unary Operation '" + TokenToString[this->_operator.type] + "' On Type '" + type->ToString() + "'", *context->current_token);
	//store here
	//only do this for ++and--
	return type;
}