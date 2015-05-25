#include "Compiler.h"


#include "Parser.h"
#include "Lexer.h"

using namespace Jet;

void Type::Load(Compiler* compiler)
{
	//recursively load
	if (loaded == true)
		return;

	if (type == Types::Class)
	{
		data->Load(compiler);
	}
	this->loaded = true;
}

void Struct::Load(Compiler* compiler)
{
	if (loaded)
		return;

	//recursively load
	std::vector<llvm::Type*> elementss;
	for (auto ii : this->members)
	{
		auto type = ii.second;
		ii.second.Load(compiler);// compiler->LookupType(ii.first);
		//s->members.push_back({ ii.second, type });
		elementss.push_back(GetType(type));
	}
	this->type = llvm::StructType::create(elementss, this->name);
	//context->parent->module->getOrInsertGlobal("testing", type);
	//add me to the list!
	//s->type = type;

	//context->parent->types[this->name] = Type(Types::Class, s);

	this->loaded = true;
}

void Function::Load(Compiler* compiler)
{
	if (this->loaded)
		return;

	this->return_type.Load(compiler);

	//std::vector<llvm::Type*> argsv;
	for (auto type : this->argst)
	{
		//auto type = context->parent->AdvanceTypeLookup(ii.first);

		//fun->argst.push_back(type);
		type.first.Load(compiler);
		this->args.push_back(GetType(type.first));
	}

	//CompilerContext* function = context->AddFunction(fname, this->args->size());// , this->varargs);
	//std::vector<llvm::Type*> Doubles(args->size(), llvm::Type::getDoubleTy(context->parent->context));
	llvm::FunctionType *ft = llvm::FunctionType::get(GetType(this->return_type), this->args, false);
	llvm::Function *f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, name, compiler->module);

	this->f = f;
	//context->parent->functions[fname] = fun;
	//ok, kinda hacky
	//int start = context->out.size();

	//ok push locals, in opposite order
	/*for (unsigned int i = 0; i < this->args->size(); i++)
	{
	auto aname = static_cast<NameExpression*>((*this->args)[i]);
	function->RegisterLocal(aname->GetName());
	}
	if (this->varargs)
	function->RegisterLocal(this->varargs->GetName());*/

	//alloc args
	auto AI = f->arg_begin();
	for (unsigned Idx = 0, e = argst.size(); Idx != e; ++Idx, ++AI) {
		// Create an alloca for this variable.
		//llvm::AllocaInst *Alloca = CreateEntryBlockAlloca(F, Args[Idx]);
		auto aname = this->argst[Idx].second;

		//llvm::IRBuilder<> TmpB(&function->f->getEntryBlock(), function->f->getEntryBlock().begin());
		//auto Alloca = TmpB.CreateAlloca(llvm::Type::getDoubleTy(function->parent->context), 0, aname);
		// Store the initial value into the alloca.
		//function->parent->builder.CreateStore(AI, Alloca);

		AI->setName(aname);

		// Add arguments to variable symbol table.
		//function->named_values[aname] = Alloca;
	}

	this->loaded = true;
}

llvm::Type* Jet::GetType(Type t)
{
	switch (t.type)
	{
	case Types::Double:
		return llvm::Type::getDoubleTy(llvm::getGlobalContext());
	case Types::Float:
		return llvm::Type::getFloatTy(llvm::getGlobalContext());
	case Types::Int:
		return llvm::Type::getInt32Ty(llvm::getGlobalContext());
	case Types::Void:
		return llvm::Type::getVoidTy(llvm::getGlobalContext());
	case Types::Char:
		return llvm::Type::getInt8Ty(llvm::getGlobalContext());
	case Types::Short:
		return llvm::Type::getInt16Ty(llvm::getGlobalContext());
	case Types::Bool:
		return llvm::Type::getInt1Ty(llvm::getGlobalContext());
	case Types::Class:
		return t.data->type;
	case Types::Pointer:
		return llvm::PointerType::get(GetType(t.base), 0);//address space, wat?
	}
	throw 7;
}

//#include "llvm/ExecutionEngine/MCJIT.h"
void Compiler::Compile(const char* code, const char* filename)
{
	//spin off children and lets compile this bitch!
	module = new llvm::Module(filename, context);

	Lexer lexer = Lexer(code, filename);
	Parser parser = Parser(&lexer);

	//printf("In: %s\n\nResult:\n", code);
	BlockExpression* result = parser.parseAll();
	//result->print();
	//printf("\n\n");
	//compile it
	//first lets create the global context!!
	//ok this will be the main entry point it initializes everything, then calls the program's entry point
	auto global = new CompilerContext(this);

	//do this for each file
	for (auto ii : result->statements)
	{
		ii->CompileDeclarations(global);
	}


	//then do this for each file
	for (auto ii : result->statements)
	{
		//branch off a new compiler
		ii->Compile(global);
	}

	auto init = global->AddFunction("global", Type(Types::Int), {});
	init->Return(global->Number(6));

	//module->dump();

	delete result;

	//go through global scope
}

CompilerContext* CompilerContext::AddFunction(const std::string& fname, Type ret, const std::vector<std::pair<Type, std::string>>& args)
{
	Function* func = parent->functions[fname];// new Function;
	if (func == 0)
	{
		//no function exists
		func = new Function;
		func->return_type = ret;
		func->argst = args;
		func->name = fname;
		for (int i = 0; i < args.size(); i++)
		{
			func->args.push_back(GetType(args[i].first));
		}

		auto n = new CompilerContext(this->parent);
		n->ft = 0;
		n->ft = llvm::FunctionType::get(GetType(ret), func->args, false);
		n->f = llvm::Function::Create(n->ft, llvm::Function::ExternalLinkage, fname, parent->module);
		n->function = func;
		func->f = n->f;
		this->parent->functions[fname] = func;

		llvm::BasicBlock *bb = llvm::BasicBlock::Create(parent->context, "entry", n->f);
		parent->builder.SetInsertPoint(bb);
		return n;
	}

	func->Load(this->parent);
	//func->return_type = ret;
	//func->argst = args;
	//func->name = fname;
	//for (int i = 0; i < args.size(); i++)
	//{
	//	func->args.push_back(GetType(args[i].first));
	//	}

	auto n = new CompilerContext(this->parent);
	n->ft = 0;
	//n->ft = llvm::FunctionType::get(GetType(ret), func->args, false);
	//n->f = llvm::Function::Create(n->ft, llvm::Function::ExternalLinkage, fname, parent->module);
	n->f = func->f;// this->parent->module->getOrInsertFunction(fname, n->ft);
	n->function = func;
	//func->f = n->f;
	//this->parent->functions[fname] = func;

	llvm::BasicBlock *bb = llvm::BasicBlock::Create(parent->context, "entry", n->f);
	parent->builder.SetInsertPoint(bb);

	return n;
}