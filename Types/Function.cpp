#include "Function.h"
#include "Types.h"
#include "Compiler.h"
#include "CompilerContext.h"
#include "Expressions.h"
#include "DeclarationExpressions.h"
#include "Lexer.h"

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
	delete this->context;
}

void Function::Load(Compilation* compiler)
{
	if (this->loaded)
		return;

	if (this->return_type->type == Types::Invalid)
		this->return_type = compiler->LookupType(this->return_type->name);

	std::vector<llvm::Type*> args;
	std::vector<llvm::Metadata*> ftypes;
	// return structs through the first argument by pointer
	if (this->return_type->type == Types::Struct)
	{
		args.push_back(return_type->GetPointerType()->GetLLVMType());
	}
	for (auto& type : this->arguments)
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
	if (this->return_type->type == Types::Struct)
	{
		ft = llvm::FunctionType::get(VoidType.GetLLVMType(), args, false);
	}
	else
	{
		ft = llvm::FunctionType::get(this->return_type->GetLLVMType(), args, false);
	}

	// todo need to know if this is a C function, if it is we shouldnt mangle
	const auto mangled_name = mangle(name, this->is_lambda, arguments, is_c_function);

	this->f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, mangled_name, compiler->module);
	switch (this->calling_convention)
	{
	case CallingConvention::StdCall:
		f->setCallingConv(llvm::CallingConv::X86_StdCall);
		break;
	case CallingConvention::FastCall:
		f->setCallingConv(llvm::CallingConv::X86_FastCall);
		break;
	case CallingConvention::ThisCall:
		f->setCallingConv(llvm::CallingConv::X86_ThisCall);
		break;
	}

	// dont add debug info for externs (todo also handle jet externs)
	if (!is_c_function && !is_extern)
	{
		llvm::DIFile* unit = compiler->debug_info.file;

		auto functiontype = compiler->debug->createSubroutineType(compiler->debug->getOrCreateTypeArray(ftypes));
		int line = this->expression ? this->expression->token.line : 0;
		llvm::DISubprogram* sp = compiler->debug->createFunction(unit, this->name, mangled_name, unit, line, functiontype, false, true, line, llvm::DINode::DIFlags::FlagPublic, false, nullptr);// , f);

		// this catches duplicates or incorrect functions
		assert(sp->describes(f));
		this->scope = sp;

		f->setSubprogram(sp);
	}

	//alloc args
	auto AI = f->arg_begin();
	// add struct return pointer argument
	if (this->return_type->type == Types::Struct)
	{
		AI->setName("return");
		AI->addAttr(llvm::Attribute::get(compiler->context, llvm::Attribute::AttrKind::StructRet));
		++AI;
	}
	for (unsigned Idx = 0, e = arguments.size(); Idx != e; ++Idx, ++AI)
	{
		auto aname = this->arguments[Idx].second;

		AI->setName(aname);

		//ok, lets watch this here, only need to do this on StdCall and non PoD(lets just do it for everything right now, who needs microoptimizations)
		//if I do this I have to use it correctly
		// add attributes for structs being passed in by pointer
		if (/*this->calling_convention == CallingConvention::StdCall &&*/ this->arguments[Idx].first->type == Types::Struct)
		{
			//AI->addAttr(llvm::Attribute::get(compiler->context, llvm::Attribute::AttrKind::ByVal));
			//AI->addAttr(llvm::Attribute::get(compiler->context, llvm::Attribute::AttrKind::Alignment, 4));
		}

		//auto D = compiler->debug->createLocalVariable(llvm::dwarf::DW_TAG_arg_variable, this->scope, aname, compiler->debug_info.file, line,
		//	this->arguments[Idx].first->GetDebugType(compiler));
		//compiler->debug->insertDeclare(AI.getNodePtrUnchecked(), D, 0, llvm::DebugLoc::get(line, 0, this->scope), &f->getBasicBlockList().front());
	}

	this->loaded = true;
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

Type* Function::GetType(Compilation* compiler)
{
	std::vector<Type*> args;
	for (unsigned int i = 0; i < this->arguments.size(); i++)
		args.push_back(this->arguments[i].first);

	return compiler->GetFunctionType(return_type, args);
}

CValue Function::Call(CompilerContext* context, const std::vector<CValue>& argsv, bool devirtualize)
{
	//virtual function calls for generators fail, need to devirtualize them
	// this shouldnt matter, all generators shouldnt be virtual
	if (this->is_generator)
		devirtualize = true;

	bool ptr_struct_return = (this->return_type->type == Types::Struct);

	//convert to an array of args for llvm
	std::vector<llvm::Value*> arg_vals;
	//if we return a struct, it is actually passed through the first argument
	if (ptr_struct_return)
	{
		auto TheFunction = context->function->f;
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

	//list of struct arguments to add attributes to
	std::vector<int> to_convert;
	int i = 0; int ai = 0;
	if (this->return_type->type == Types::Struct)
	{
		i = 1;
	}

	// for every argument that is a struct, pass it by pointer instead of by value
	for (auto& ii : argsv)
	{
		if (ii.val->getType()->isStructTy() && ii.type->type != Types::Array)
		{
			//convert it to a pointer yo
			/*auto TheFunction = context->function->f;
			llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
				TheFunction->getEntryBlock().begin());
			auto alloc = TmpB.CreateAlloca(ii.val->getType(), 0, "arg_pass_tmp");
			alloc->setAlignment(4);

			//construct it
			context->Construct(CValue(ii.type->GetPointerType(), alloc), 0);// todo do I actually need to construct it in all cases?
			//seems like a possible memory leak location

			//extract the types from the function
			auto type = this->arguments[ai].first;
			if (type->type == Types::Struct)
			{
				auto funiter = type->data->functions.find("=");
				//todo: search through multimap to find one with the right number of args
				if (funiter != type->data->functions.end() && funiter->second->arguments.size() == 2)
				{
					// call the copy constructor
					Function* fun = funiter->second;
					fun->Load(context->root);
					if (ii.pointer == 0)
						context->root->Error("Cannot convert to reference", *context->current_token);

					auto pt = type->GetPointerType();
					std::vector<CValue> argsv = { CValue(pt, alloc), CValue(pt, ii.pointer) };

					fun->Call(context, argsv, true);
				}
				else if (type->data->is_class)
				{
					context->root->Error("Cannot copy class '" + type->data->name + "' unless it has a copy operator.", *context->current_token);
				}
				else
				{
					//copy by value
					//was ii.val, but this makes more sense
					auto sptr = context->root->builder.CreatePointerCast(ii.pointer, context->root->CharPointerType->GetLLVMType());
					auto dptr = context->root->builder.CreatePointerCast(alloc, context->root->CharPointerType->GetLLVMType());
					context->root->builder.CreateMemCpy(dptr, 0, sptr, 0, type->GetSize());// todo properly handle alignment
				}
			}
			else
			{
				throw 7;//how does this even get hit
			}*/

            if (ii.pointer == 0)
				context->root->Error("Cannot convert to reference", *context->current_token);

			arg_vals[i] = ii.pointer;
			to_convert.push_back(i);
		}
		i++;
		ai++;
	}

	//if we are the upper level of the inheritance tree, devirtualize
	//if we are a virtual call, do that
	llvm::CallInst* call;
	if (this->is_virtual && devirtualize == false)
	{
		// calculate the offset to the virtual table pointer in the struct
		auto gep = context->root->builder.CreateGEP(argsv[0].val, 
			{ context->root->builder.getInt32(0), context->root->builder.getInt32(this->virtual_table_location) }, 
			"get_vtable");

		// then load that pointer to get the vtable pointer
		llvm::Value* ptr = context->root->builder.CreateLoad(gep);

		// index into the vtable for this particular function
		ptr = context->root->builder.CreateGEP(ptr, { context->Integer(this->virtual_offset).val }, "get_offset_in_vtable");

		// load the function pointer from the vtable
		ptr = context->root->builder.CreateLoad(ptr);

		//then cast it to the correct function type
		auto func = context->root->builder.CreatePointerCast(ptr, this->GetType(context->root)->GetLLVMType());

		//then call it
		call = context->root->builder.CreateCall(func, arg_vals);
		//call->setCallingConv(fun->f->getCallingConv());
	}
	else
	{
		call = context->root->builder.CreateCall(this->f, arg_vals);
		call->setCallingConv(this->f->getCallingConv());
	}

	//add attributes
	for (auto ii : to_convert)
	{
		//auto& ctext = context->root->builder.getContext();
		//call->addParamAttr(ii, llvm::Attribute::get(ctext, llvm::Attribute::AttrKind::ByVal));
		//call->addParamAttr(ii, llvm::Attribute::get(ctext, llvm::Attribute::AttrKind::Alignment, 4));
	}

	if (ptr_struct_return)
	{
		auto& ctext = context->root->builder.getContext();
		call->addParamAttr(0, llvm::Attribute::get(ctext, llvm::Attribute::AttrKind::StructRet));
		return CValue(this->return_type, 0, arg_vals[0]);
	}
	return CValue(this->return_type, call);
}
