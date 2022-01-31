#include "Function.h"
#include "Types.h"
#include "../Compiler.h"
#include "../CompilerContext.h"
#include <expressions/Expressions.h>
#include <expressions/DeclarationExpressions.h>
#include "../Lexer.h"

using namespace Jet;

static unsigned int uuid = 5;
std::string mangle(const std::string& name, bool is_lambda, const std::vector<std::pair<Type*, std::string>>& arguments, bool is_c_function)
{
	// handle main
	if (name == "main")
	{
		return name;
	}
	if (name == "_init")
	{
		return name;
	}
	if (is_lambda)
	{
		return name + std::to_string(uuid++);
	}
	if (is_c_function)
	{
		return name;
	}
	// todo actually mangle
	return name + "_" + std::to_string(arguments.size());
}

Function::~Function()
{
	delete context_;
}

void Function::Load(Compilation* compiler)
{
	if (loaded_)
		return;

	if (return_type_->type == Types::Invalid)
		return_type_ = compiler->LookupType(return_type_->name);

	std::vector<llvm::Type*> args;
	std::vector<llvm::Metadata*> ftypes;
	// return structs through the first argument by pointer
	if (return_type_->type == Types::Struct)
	{
		args.push_back(return_type_->GetPointerType()->GetLLVMType());
	}
	for (auto& type : arguments_)
	{
		// lookup any unloaded types
		if (type.first->type == Types::Invalid)
			type.first = compiler->LookupType(type.first->name);

		// make sure the type is loaded then add it to the list
		type.first->Load(compiler);
		args.push_back(type.first->GetLLVMType());

		//add debug stuff
		ftypes.push_back(type.first->GetDebugType(compiler));

		// pass struct arguments and references by pointer
		if (/*this->calling_convention == CallingConvention::StdCall &&*/
            type.first->type == Types::Struct)
			args[args.size() - 1] = args[args.size() - 1]->getPointerTo();
	}

	// Determine the return type and final llvm function type
	llvm::FunctionType *ft;
	if (return_type_->type == Types::Struct)
	{
		ft = llvm::FunctionType::get(compiler->VoidType->GetLLVMType(), args, false);
	}
	else
	{
		ft = llvm::FunctionType::get(return_type_->GetLLVMType(), args, false);
	}

	// todo need to know if this is a C function, if it is we shouldnt mangle
	const auto mangled_name = mangle(name_, is_lambda_, arguments_, is_c_function_);

    //printf("mangling %s to %s\n", name.c_str(), mangled_name.c_str());

	f_ = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, mangled_name, compiler->module);
	switch (calling_convention_)
	{
	case CallingConvention::StdCall:
		f_->setCallingConv(llvm::CallingConv::X86_StdCall);
		break;
	case CallingConvention::FastCall:
		f_->setCallingConv(llvm::CallingConv::X86_FastCall);
		break;
	case CallingConvention::ThisCall:
		f_->setCallingConv(llvm::CallingConv::X86_ThisCall);
		break;
	}

	// dont add debug info for externs or c functions
	if (!is_c_function_ && !is_extern_)
	{
		llvm::DIFile* unit = compiler->debug_info.file;

		auto functiontype = compiler->debug->createSubroutineType(compiler->debug->getOrCreateTypeArray(ftypes));
		int line = expression_ ? expression_->token.line : 0;
		llvm::DISubprogram* sp = compiler->debug->createFunction(unit, name_, mangled_name, unit, line, functiontype, false, true, line, llvm::DINode::DIFlags::FlagPublic, false, nullptr);// , f);

		// this catches duplicates or incorrect functions
		assert(sp->describes(f_));
		scope_ = sp;

		f_->setSubprogram(sp);
	}

	//alloc args
	auto AI = f_->arg_begin();
	// add struct return pointer argument
	if (return_type_->type == Types::Struct)
	{
		AI->setName("return");
		AI->addAttr(llvm::Attribute::get(compiler->context, llvm::Attribute::AttrKind::StructRet));
		++AI;
	}
	for (unsigned Idx = 0, e = arguments_.size(); Idx != e; ++Idx, ++AI)
	{
		auto aname = arguments_[Idx].second;

		AI->setName(aname);

		//ok, lets watch this here, only need to do this on StdCall and non PoD(lets just do it for everything right now, who needs microoptimizations)
		//if I do this I have to use it correctly
		// add attributes for structs being passed in by pointer
		if (/*this->calling_convention == CallingConvention::StdCall &&*/ arguments_[Idx].first->type == Types::Struct)
		{
			//AI->addAttr(llvm::Attribute::get(compiler->context, llvm::Attribute::AttrKind::ByVal));
			//AI->addAttr(llvm::Attribute::get(compiler->context, llvm::Attribute::AttrKind::Alignment, 4));
		}

		//auto D = compiler->debug->createLocalVariable(llvm::dwarf::DW_TAG_arg_variable, this->scope, aname, compiler->debug_info.file, line,
		//	this->arguments[Idx].first->GetDebugType(compiler));
		//compiler->debug->insertDeclare(AI.getNodePtrUnchecked(), D, 0, llvm::DebugLoc::get(line, 0, this->scope), &f->getBasicBlockList().front());
	}

	loaded_ = true;
}

Function* Function::Instantiate(Compilation* compiler, const std::vector<Type*>& types)
{
	/*//register the types
	int i = 0;
	for (auto ii : this->templates)
	{
	//check if traits match
	if (types[i]->MatchesTrait(compiler, ii.first) == false)
	Error("Type '" + types[i]->name + "' doesn't match Trait '" + ii.first->name + "'", *compiler->current_function->current_token);

	//lets be stupid and just register the type
	//CHANGE ME LATER, THIS OVERRIDES TYPES, OR JUST RESTORE AFTER THIS
	compiler->types[ii.second] = types[i++];
	}

	//duplicate and load
	Function* str = new Function;
	//build members
	for (auto ii : this->expression->members)
	{
	if (ii.type == StructMember::VariableMember)
	{
	auto type = compiler->AdvanceTypeLookup(ii.variable.first.text);

	str->members.push_back({ ii.variable.second.text, ii.variable.first.text, type });
	}
	}

	str->template_base = this->data;
	str->name = this->data->name + "<";
	for (int i = 0; i < this->data->templates.size(); i++)
	{
	str->name += types[i]->ToString();
	if (i < this->data->templates.size() - 1)
	str->name += ',';
	}
	str->name += ">";
	str->expression = this->data->expression;

	Type* t = new Type(str->name, Types::Struct, str);
	t->Load(compiler);

	return t; */
	return 0;
}

Type* Function::GetType(Compilation* compiler) const
{
	std::vector<Type*> args;
	for (unsigned int i = 0; i < arguments_.size(); i++)
		args.push_back(arguments_[i].first);

	return compiler->GetFunctionType(return_type_, args);
}

CValue Function::Call(CompilerContext* context, const std::vector<CValue>& argsv,
  const bool devirtualize, bool bonus_arg)
{
    this->Load(context->root);

    if (!type_)
    {
        type_ = this->GetType(context->root)->function;
    }

	//virtual function calls for generators fail, need to devirtualize them
	// this shouldnt matter, all generators shouldnt be virtual
    bool dv = devirtualize;
    if (is_generator_)
    {
        dv = true;
    }

    return type_->Call(context, f_, argsv, dv, this, bonus_arg);
}

CValue FunctionType::Call(CompilerContext* context, llvm::Value* fn, const std::vector<CValue>& argsv,
  const bool devirtualize, const Function* f, const bool bonus_arg)
{
	bool ptr_struct_return = (return_type->type == Types::Struct);

	//convert to an array of args for llvm
	std::vector<llvm::Value*> arg_vals;
	//if we return a struct, it is actually passed through the first argument
	if (ptr_struct_return)
	{
		auto TheFunction = context->function->f_;
		llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
			TheFunction->getEntryBlock().begin());
		auto alloc = TmpB.CreateAlloca(this->return_type->GetLLVMType(), 0, "return_pass_tmp");
		//alloc->setAlignment(4);
		arg_vals.push_back(alloc);
	}
	for (auto ii : argsv)
	{
		arg_vals.push_back(ii.val);
	}

    // check for correct number of arguments
    int expected_args = bonus_arg ? args.size() + 1 : args.size();
    if (expected_args != argsv.size())
    {
        context->root->Error("Function expected " + std::to_string(expected_args) + " arguments, got " + std::to_string(argsv.size()), *context->current_token);
    }

	//list of struct arguments to add attributes to
	//std::vector<int> to_convert;
	int i = 0; int ai = 0;
	if (this->return_type->type == Types::Struct)
	{
		i = 1;
	}

    try
    {
	    // for every argument that is a struct, pass it by pointer instead of by value
	    for (auto& ii : argsv)
	    {
            // Cast first
            if (ai > args.size() - 1)
            {
                arg_vals[i] = ii.val;
                i++; ai++;
                continue;
            }
            
            CValue casted = context->DoCast(this->args[ai], ii);
		    if (casted.type->type == Types::Struct || casted.type->type == Types::Union)
		    {
                if (casted.pointer == 0)
				    context->root->Error("Cannot convert to reference", *context->current_token);

			    arg_vals[i] = casted.pointer;
		    }
            else
            {
                if (!casted.val)
                {
                    casted.val = context->root->builder.CreateLoad(casted.pointer, "autodereference");
                }
                arg_vals[i] = casted.val;
            }
		    i++;
		    ai++;
	    }
    }
    catch (int i)
    {
        Token t;
        std::string ns;
        if (f && f->expression_)
        {
            t = f->expression_->name;
            ns = f->expression_->GetHumanReadableNamespace();
        }
        else if (f && f->extern_expression_)
        {
            t = f->extern_expression_->name;
            ns = f->extern_expression_->GetHumanReadableNamespace();
            if (ns.length()) { ns += "::"; }
            if (f->extern_expression_->Struct.length()) { ns += f->extern_expression_->Struct; }
        }
        else
        {
            // its probably a function pointer
            t.text = this->ToString();
        }
        if (ns.length()) { ns += "::"; }
        context->root->Info("For argument " + std::to_string(ai + 1) + " of '" + ns + t.text + "'", t);
        throw i;
    }

	//if we are the upper level of the inheritance tree, devirtualize
	//if we are a virtual call, do that
	llvm::CallInst* call;
	if (f && f->is_virtual_ && devirtualize == false)
	{
		// calculate the offset to the virtual table pointer in the struct
		auto gep = context->root->builder.CreateGEP(argsv[0].val, 
			{ context->root->builder.getInt32(0), context->root->builder.getInt32(f->virtual_table_location_) }, 
			"get_vtable");

		// then load that pointer to get the vtable pointer
		llvm::Value* ptr = context->root->builder.CreateLoad(gep);

		// index into the vtable for this particular function
		ptr = context->root->builder.CreateGEP(ptr, { context->Integer(f->virtual_offset_).val }, "get_offset_in_vtable");

		// load the function pointer from the vtable
		ptr = context->root->builder.CreateLoad(ptr);

		//then cast it to the correct function type
		auto func = context->root->builder.CreatePointerCast(ptr, f->GetType(context->root)->GetLLVMType());

		//then call it
		call = context->root->builder.CreateCall(func, arg_vals);
		//call->setCallingConv(fun->f->getCallingConv());
	}
	else
	{
		call = context->root->builder.CreateCall(fn, arg_vals);
		//call->setCallingConv(this->f->getCallingConv());
	}

	//add attributes
	/*for (auto ii : to_convert)
	{
		//auto& ctext = context->root->builder.getContext();
		//call->addParamAttr(ii, llvm::Attribute::get(ctext, llvm::Attribute::AttrKind::ByVal));
		//call->addParamAttr(ii, llvm::Attribute::get(ctext, llvm::Attribute::AttrKind::Alignment, 4));
	}*/

	if (ptr_struct_return)
	{
		auto& ctext = context->root->builder.getContext();
		call->addParamAttr(0, llvm::Attribute::get(ctext, llvm::Attribute::AttrKind::StructRet));
		return CValue(this->return_type, 0, arg_vals[0]);
	}
	return CValue(this->return_type, call);
}
