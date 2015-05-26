#include "Compiler.h"


#include "Parser.h"
#include "Lexer.h"

using namespace Jet;

Type Jet::VoidType(Types::Void);
Type Jet::BoolType(Types::Bool);
Type Jet::DoubleType(Types::Double);
Type Jet::IntType(Types::Int);

void Jet::Error(const std::string& msg, Token token)
{
	int startrow = token.column;
	printf("[error] %d:%d to %d:%d: %s\n[error] >>>%s\n\n", token.line, startrow, token.line, startrow+token.text.length(), msg.c_str(), "some code here yo");
	throw 7;
}

void Type::Load(Compiler* compiler)
{
	//recursively load
	if (loaded == true)
		return;

	if (type == Types::Class)
	{
		data->Load(compiler);
	}
	else if (type == Types::Invalid)
	{
		//get a good error here!!!
		Error("Tried to use undefined type", *compiler->current_function->current_token);
		//printf("Tried to use undefined type\n");
		//throw 7;
	}
	else if (type == Types::Pointer)
	{
		//load recursively
		this->base->Load(compiler);
	}
	this->loaded = true;
}

std::string Type::ToString()
{
	switch (type)
	{
	case Types::Class:
		return this->data->name;
	case Types::Pointer:
		return this->base->ToString() + "*";
	case Types::Bool:
		return "Bool";
	case Types::Char:
		return "Char";
	case Types::Int:
		return "Int";
	case Types::Float:
		return "Float";
	case Types::Double:
		return "Double";
	case Types::Short:
		return "Short";
	case Types::Void:
		return "Void";
	}
}

void Struct::Load(Compiler* compiler)
{
	if (this->loaded)
		return;

	//recursively load
	std::vector<llvm::Type*> elementss;
	for (auto ii : this->members)
	{
		auto type = ii.second;
		ii.second->Load(compiler);

		elementss.push_back(GetType(type));
	}
	this->type = llvm::StructType::create(elementss, this->name);

	this->loaded = true;
}

void Function::Load(Compiler* compiler)
{
	if (this->loaded)
		return;

	this->return_type->Load(compiler);

	for (auto type : this->argst)
	{
		type.first->Load(compiler);
		this->args.push_back(GetType(type.first));
	}

	llvm::FunctionType *ft = llvm::FunctionType::get(GetType(this->return_type), this->args, false);
	this->f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, name, compiler->module);

	//alloc args
	auto AI = f->arg_begin();
	for (unsigned Idx = 0, e = argst.size(); Idx != e; ++Idx, ++AI) 
	{
		auto aname = this->argst[Idx].second;

		AI->setName(aname);
	}

	this->loaded = true;
}

llvm::Type* Jet::GetType(Type* t)
{
	switch (t->type)
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
		return t->data->type;
	case Types::Pointer:
		return llvm::PointerType::get(GetType(t->base), 0);//address space, wat?
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

	int errors = 0;
	//then do this for each file
	for (auto ii : result->statements)
	{
		//catch any exceptions
		try
		{
			ii->Compile(global);
		}
		catch (...)
		{
			printf("Exception Compiling Line\n");
			errors++;
		}
	}

	auto init = global->AddFunction("global", &IntType, {});
	init->Return(global->Number(6));
	//todo: put intializers here and have this call main()
	delete result;

	if (errors > 0)
	{
		printf("Compiling Failed: %d Errors Found\n", errors);
		delete module;
		module = 0;
	}

	//go through global scope
}

CompilerContext* CompilerContext::AddFunction(const std::string& fname, Type* ret, const std::vector<std::pair<Type*, std::string>>& args)
{
	Function* func = parent->functions[fname];
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
		
		auto ft = llvm::FunctionType::get(GetType(ret), func->args, false);
		n->f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, fname, parent->module);
		n->function = func;
		func->f = n->f;
		this->parent->functions[fname] = func;

		llvm::BasicBlock *bb = llvm::BasicBlock::Create(parent->context, "entry", n->f);
		parent->builder.SetInsertPoint(bb);
		return n;
	}

	func->Load(this->parent);


	auto n = new CompilerContext(this->parent);
	n->f = func->f;
	n->function = func;

	llvm::BasicBlock *bb = llvm::BasicBlock::Create(parent->context, "entry", n->f);
	parent->builder.SetInsertPoint(bb);

	this->parent->current_function = n;
	return n;
}