#include "Function.h"
#include "Types.h"
#include "Compiler.h"
#include "CompilerContext.h"
#include "Expressions.h"
#include "Lexer.h"

using namespace Jet;

void Function::Load(Compilation* compiler)
{
	if (this->loaded)
		return;

	this->return_type->Load(compiler);

	std::vector<llvm::Type*> args;
	std::vector<llvm::Metadata*> ftypes;
	for (auto type : this->arguments)
	{
		type.first->Load(compiler);
		args.push_back(::GetType(type.first));

		//add debug stuff
		ftypes.push_back(type.first->GetDebugType(compiler));
	}

	llvm::FunctionType *ft = llvm::FunctionType::get(::GetType(this->return_type), /*this->*/args, false);

	this->f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, name, compiler->module);
	compiler->functions.push_back(this);
	llvm::DIFile* unit = compiler->debug_info.file;

	auto functiontype = compiler->debug->createSubroutineType(unit, compiler->debug->getOrCreateTypeArray(ftypes));
	int line = this->expression ? this->expression->token.line : 0;
	llvm::DISubprogram* sp = compiler->debug->createFunction(unit, this->name, this->name, unit, line, functiontype, false, true, line, 0, false, f);

	//FContext, Name, StringRef(), Unit, LineNo, 0, false, f);
	//for some reason struct member functions derp
	//CreateFunctionType(Args.size(), Unit), false /* internal linkage */,
	//true /* definition */, ScopeLine, DINode::FlagPrototyped, false, F);
	assert(sp->describes(f));
	this->scope = sp;
	compiler->builder.SetCurrentDebugLocation(llvm::DebugLoc::get(5, 1, 0));
	//compiler->debug->finalize();
	//compiler->module->dump();

	//alloc args
	auto AI = f->arg_begin();
	for (unsigned Idx = 0, e = arguments.size(); Idx != e; ++Idx, ++AI)
	{
		auto aname = this->arguments[Idx].second;

		AI->setName(aname);
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
	for (int i = 0; i < this->arguments.size(); i++)
		args.push_back(this->arguments[i].first);
	
	return compiler->GetFunctionType(return_type, args);
}
