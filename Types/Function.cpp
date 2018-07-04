#include "Function.h"
#include "Types.h"
#include "Compiler.h"
#include "CompilerContext.h"
#include "Expressions.h"
#include "DeclarationExpressions.h"
#include "Lexer.h"

using namespace Jet;

void Function::Load(Compilation* compiler)
{
	if (this->loaded)
		return;

	if (this->return_type->type == Types::Invalid)
		this->return_type = compiler->LookupType(this->return_type->name);
	
	std::vector<llvm::Type*> args;
	std::vector<llvm::Metadata*> ftypes;
	for (auto& type : this->arguments)
	{
		if (type.first->type == Types::Invalid)
			type.first = compiler->LookupType(type.first->name);

		type.first->Load(compiler);
		args.push_back(type.first->GetLLVMType());

		//add debug stuff
		ftypes.push_back(type.first->GetDebugType(compiler));

		if (/*this->calling_convention == CallingConvention::StdCall &&*/ type.first->type == Types::Struct)
			args[args.size() - 1] = args[args.size() - 1]->getPointerTo();
	}

	llvm::FunctionType *ft = llvm::FunctionType::get(this->return_type->GetLLVMType(), args, false);

	this->f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, name, compiler->module);
	//need to add way to specify calling convention
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
	//ok, calling convention setting doesnt seem to work right
	//	winapi functions fail on return it seems :S
	llvm::DIFile* unit = compiler->debug_info.file;

	auto functiontype = compiler->debug->createSubroutineType(compiler->debug->getOrCreateTypeArray(ftypes));
	int line = this->expression ? this->expression->token.line : 0;
	llvm::DISubprogram* sp = compiler->debug->createFunction(unit, this->name, this->name, unit, line, functiontype, false, true, line, llvm::DINode::DIFlags::FlagPublic, false, nullptr);// , f);

	assert(sp->describes(f));
	this->scope = sp;
	compiler->builder.SetCurrentDebugLocation(llvm::DebugLoc::get(5, 1, 0));

	//alloc args
	auto AI = f->arg_begin();
	for (unsigned Idx = 0, e = arguments.size(); Idx != e; ++Idx, ++AI)
	{
		auto aname = this->arguments[Idx].second;

		AI->setName(aname);

		//ok, lets watch this here, only need to do this on StdCall and non PoD(lets just do it for everything right now, who needs microoptimizations)
		//if I do this I have to use it correctly
		if (/*this->calling_convention == CallingConvention::StdCall &&*/ this->arguments[Idx].first->type == Types::Struct)
		{
			AI->addAttr(llvm::Attribute::get(compiler->context, llvm::Attribute::AttrKind::ByVal));
			AI->addAttr(llvm::Attribute::get(compiler->context, llvm::Attribute::AttrKind::Alignment, 4));
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
