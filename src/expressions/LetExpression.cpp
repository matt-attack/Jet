#include "LetExpression.h"
#include "DeclarationExpressions.h"

using namespace Jet;

CValue LetExpression::Compile(CompilerContext* context)
{
	context->CurrentToken(&(*_names)[0].name);

	// If im in global scope (not in a function), add a global variable
	if (this->parent->parent == 0 || dynamic_cast<NamespaceExpression*>(this->parent->parent))
	{
        std::string name = GetNamespaceQualifier() + this->_names->front().name.text;
		if (this->_names->front().type.text.length() == 0)
		{
			context->root->Error("Cannot infer type for a global variable", token);
		}

		auto type = context->root->LookupType(this->_names->front().type.text);

		//should I add a constructor?
		llvm::Constant* cval = 0;
		if (this->_right && this->_right->size() > 0)
		{
			auto val = this->_right->front().second->Compile(context);
			cval = llvm::dyn_cast<llvm::Constant>(val.val);
			if (cval == 0)
				context->root->Error("Cannot instantiate global with non constant value", token);
		}

		if (this->_names->front().type.text.back() == ']')
		{
			std::string len = this->_names->front().type.text;
			len = len.substr(len.find_first_of('[') + 1);
		    if (is_const && len.c_str())
		    {
			    context->root->Error("Cannot declare a const global array variable.", token);
		    }

			context->root->AddGlobal(this->_names->front().name.text, type->base, std::atoi(len.c_str()), 0, false, is_const);
		}
		else
		{
			context->root->AddGlobal(this->_names->front().name.text, type, 0, cval, false, is_const);
		}

		return CValue(context->root->VoidType, 0);
	}
    else
    {
        context->SetDebugLocation(this->token);
    }

	bool needs_destruction = false;
	int i = 0;
	for (const auto& ii : *this->_names)
    {
		auto aname = ii.name.text;

		Type* type = 0;
		CValue val(0, 0);
		llvm::AllocaInst* Alloca = 0;
		if (ii.type.text.length())
		{
			context->CurrentToken(&ii.type);
			type = context->root->LookupType(ii.type.text);
			context->CurrentToken(&(*_names)[0].name);
			if (this->_right)
				val = (*this->_right)[i++].second->Compile(context);
		}
		else if (this->_right)
		{
			val = (*this->_right)[i++].second->Compile(context);
			type = val.type;
		}

		if (context->function->is_generator_)// Add arguments to variable symbol table.
		{
			//find the already added type with the same name
			auto ty = context->function->arguments_[0].first->base;
			auto var_ptr = context->function->generator_.variable_geps[context->function->generator_.var_num++];

			if (this->_right)
			{
				val = context->DoCast(type, val);
				context->root->builder.CreateStore(val.val, var_ptr);
			}

			//output debug info
			llvm::DIFile* unit = context->root->debug_info.file;
			type->Load(context->root);
			llvm::DILocalVariable* D = context->root->debug->createAutoVariable(context->function->scope_, aname, unit, ii.name.line,
				type->GetDebugType(context->root));

			llvm::Instruction *Call = context->root->debug->insertDeclare(
				var_ptr, D, context->root->debug->createExpression(), llvm::DebugLoc::get(this->token.line, this->token.column, context->function->scope_), context->root->builder.GetInsertBlock());
			Call->setDebugLoc(llvm::DebugLoc::get(ii.name.line, ii.name.column, context->function->scope_));

			//still need to do store
			// todo this probably needs to support things with destructors..
            context->CurrentToken(&ii.name);
			context->RegisterLocal(aname, CValue(type, 0, var_ptr), false, is_const);
			continue;
		}
		else if (ii.type.text.length() > 0)//type was specified
		{
			context->CurrentToken(&ii.type);

			type = context->root->LookupType(ii.type.text);

			auto TheFunction = context->function->f_;
			llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
				TheFunction->getEntryBlock().begin());

			if (type->type == Types::Struct && type->data->templates.size() > 0)
			{
				context->root->Error("Missing template arguments for type '" + type->ToString() + "'", ii.type);
			}
			else if (type->type == Types::InternalArray)
			{
				if (this->_right)
				{
					context->root->Error("Cannot assign to a sized array type!", ii.name);
				}

				Alloca = TmpB.CreateAlloca(type->GetLLVMType(), TmpB.getInt32(1), aname);
			}
			else if (type->type == Types::Array)
			{
				//if (type->size)
				//	needs_destruction = true;

				int length = 0;// type->size;

				auto str_type = context->root->GetArrayType(type->base);
				//type = str_type;
				//alloc the struct for it
				Alloca = TmpB.CreateAlloca(str_type->GetLLVMType(), TmpB.getInt32(1), aname);

				//allocate the array
				auto size = TmpB.getInt32(length);
				auto arr = TmpB.CreateAlloca(type->base->GetLLVMType(), size, aname + ".array");
				//store size
				auto size_p = TmpB.CreateGEP(Alloca, { TmpB.getInt32(0), TmpB.getInt32(0) });
				TmpB.CreateStore(size, size_p);

				//store pointer (todo write zero?)
				//auto pointer_p = TmpB.CreateGEP(Alloca, { TmpB.getInt32(0), TmpB.getInt32(1) });
				//TmpB.CreateStore(arr, pointer_p);
			}
			else if (type->GetBaseType()->type == Types::Trait)
			{
				context->root->Error("Cannot instantiate trait", ii.name);
			}
			else
			{
				Alloca = TmpB.CreateAlloca(type->GetLLVMType(), 0, aname);
			}

			if (type->GetSize() >= 4)
				Alloca->setAlignment(4);

			// Store the initial value into the alloca.
			if (this->_right)
			{
				CValue alloc(type->GetPointerType(), Alloca);
                // properly handle constructors
                if (type->type == Types::Struct && type != val.type)
                {
                    context->DoCast(type, val, false, Alloca);
                }
                else
                {
                    if (type->type == Types::Struct)
                    {
                        context->Construct(CValue(type->GetPointerType(), Alloca), 0);
                    }
				    context->Store(alloc, val);// todo copy constructor
                }
			}
		}
		else if (this->_right)
		{
			//need to move allocas outside of the loop and into the main body
			auto TheFunction = context->function->f_;
			llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
				TheFunction->getEntryBlock().begin());

			Alloca = TmpB.CreateAlloca(val.type->GetLLVMType(), 0, aname);

			if (val.type->GetSize() >= 4)
				Alloca->setAlignment(4);

            if (type->type == Types::Struct)
            {
                context->Construct(CValue(type->GetPointerType(), Alloca), 0);
            }

			CValue alloc(val.type->GetPointerType(), Alloca);
			context->Store(alloc, val);// todo copy constructor
		}
		else
		{
			context->root->Error("Cannot infer variable type without a value!", ii.name);
		}

		// Add debug info
		llvm::DIFile* unit = context->root->debug_info.file;
		type->Load(context->root);
		llvm::DILocalVariable* D = context->root->debug->createAutoVariable(context->function->scope_, aname, unit, ii.name.line,
			type->GetDebugType(context->root));

		llvm::Instruction *declare = context->root->debug->insertDeclare(
			Alloca, D, context->root->debug->createExpression(), llvm::DebugLoc::get(this->token.line, this->token.column, context->function->scope_), context->root->builder.GetInsertBlock());
		declare->setDebugLoc(llvm::DebugLoc::get(ii.name.line, ii.name.column, context->function->scope_));

        context->CurrentToken(&ii.name);
		context->RegisterLocal(aname, CValue(type, 0, Alloca), true, is_const);

		//construct it!
		if (this->_right == 0)
		{
			if (type->type == Types::Struct)
			{
				context->Construct(CValue(type->GetPointerType(), Alloca), 0);
			}
            else if (type->type == Types::InternalArray && type->base->type == Types::Struct)
            {
				auto loc = context->root->builder.CreateGEP(Alloca, { context->root->builder.getInt32(0),  context->root->builder.getInt32(0) });
				//auto size = context->root->builder.CreateLoad(loc);

				//auto ptr = context->root->builder.CreateGEP(Alloca, { context->root->builder.getInt32(0), context->root->builder.getInt32(1) });
				//ptr = context->root->builder.CreateLoad(ptr);
				context->Construct(CValue(type->base->GetPointerType(), loc), context->root->builder.getInt32(type->size));
            }
			else if (type->type == Types::Array && type->base->type == Types::Struct)
			{
				//todo lets move this junk into construct so we dont have to do this in multiple places
				auto loc = context->root->builder.CreateGEP(Alloca, { context->root->builder.getInt32(0), context->root->builder.getInt32(0) });
				auto size = context->root->builder.CreateLoad(loc);

				auto ptr = context->root->builder.CreateGEP(Alloca, { context->root->builder.getInt32(0), context->root->builder.getInt32(1) });
				ptr = context->root->builder.CreateLoad(ptr);
				context->Construct(CValue(type->base->GetPointerType(), ptr), size);
			}
		}
	}

	return CValue(context->root->VoidType, 0);
}
