#include "Compiler.h"

#include "Source.h"
#include "Parser.h"
#include "Lexer.h"

using namespace Jet;

Type Jet::VoidType(Types::Void);
Type Jet::BoolType(Types::Bool);
Type Jet::DoubleType(Types::Double);
Type Jet::IntType(Types::Int);

//this is VERY TERRIBLE remove later
Source* current_source = 0;

//add underlines for the erroring symbol on a third line
//start using this in parser, will need to remove the exception, but it is still needed later
void Jet::Error(const std::string& msg, Token token)
{
	int startrow = token.column - token.text.length();
	int endrow = token.column;
	std::string code = current_source->GetLine(token.line);
	std::string underline = "";
	for (int i = 0; i < code.length(); i++)
	{
		if (code[i] == '\t')
			underline += '\t';
		else if (i >= startrow && i < endrow)
			underline += '~';
		else
			underline += ' ';
	}
	printf("[error] %d:%d to %d:%d: %s\n[error] >>>%s\n[error] >>>%s\n\n", token.line, startrow, token.line, endrow, msg.c_str(), code.c_str(), underline.c_str());
	throw 7;
}
//make it so this can get the source code to give a more useful error message
void Jet::ParserError(const std::string& msg, Token token)
{
	int startrow = token.column - token.text.length();
	int endrow = token.column;
	std::string code = current_source->GetLine(token.line);

	printf("[error] %d:%d to %d:%d: %s\n[error] >>>%s\n\n", token.line, startrow, token.line, endrow, msg.c_str(), code.c_str());
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
		Error("Tried To Use Undefined Type", *compiler->current_function->current_token);
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
		return "bool";
	case Types::Char:
		return "char";
	case Types::Int:
		return "int";
	case Types::Float:
		return "float";
	case Types::Double:
		return "double";
	case Types::Short:
		return "short";
	case Types::Void:
		return "void";
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
	case Types::Array:
		return llvm::ArrayType::get(GetType(t->base), t->size);
	case Types::Pointer:
		return llvm::PointerType::get(GetType(t->base), 0);//address space, wat?
	}
	throw 7;
}

#include "JIT.h"
MCJITHelper* JITHelper;

#include <direct.h>

#include <fstream>
#include <filesystem>
void Compiler::Compile(const char* projectfile)
{
	std::vector<std::string> files;

	//ok, lets parse the jp file
	std::ifstream pf(projectfile, std::ios::in | std::ios::binary);
	while (pf.peek() != EOF)
	{
		std::string file;//read in a filename
		while (pf.peek() != ' ' && pf.peek() != EOF && pf.peek() != '\r' && pf.peek() != '\n' && pf.peek() != '\t')
			file += pf.get();

		pf.get();
		if (file.length() > 0)
			files.push_back(file);
	}

	char olddir[500];
	getcwd(olddir, 500);
	std::string path = projectfile;
	int i = path.length();
	for (; i >= 0; i--)
		if (path[i] == '\\' || path[i] == '/')
			break;
	path = path.substr(0, i);
	chdir(path.c_str());

	std::map<std::string, Source*> sources;
	for (auto file : files)
	{
		std::ifstream t(file, std::ios::in | std::ios::binary);
		if (t)
		{
			t.seekg(0, std::ios::end);    // go to the end
			std::streamoff length = t.tellg();           // report location (this is the length)
			t.seekg(0, std::ios::beg);    // go back to the beginning
			char* buffer = new char[length + 1];    // allocate memory for a buffer of appropriate dimension
			t.read(buffer, length);       // read the whole file into the buffer
			buffer[length] = 0;
			t.close();

			sources[file] = new Source(buffer, file);
		}
		else
		{
			printf("Could not find file!");
			throw 7;
		}
	}


	JITHelper = new MCJITHelper(this->context);

	//spin off children and lets compile this!
	module = JITHelper->getModuleForNewFunction();// new llvm::Module(filename, context);


	//compile it
	//first lets create the global context!!
	//ok this will be the main entry point it initializes everything, then calls the program's entry point
	int errors = 0;
	auto global = new CompilerContext(this);

	std::map<std::string, BlockExpression*> asts;
	for (auto file : sources)
	{
		current_source = file.second;
		Lexer lexer = Lexer(file.second);
		Parser parser = Parser(&lexer);

		BlockExpression* result = 0;
		try
		{
			result = parser.parseAll();
		}
		//catch (CompilerException c)
		//{
		//	printf("t");
		//}
		catch (...)
		{
			printf("Compilation Stopped, Parser Error\n");
			errors = 1;
			goto error;
		}
		asts[file.first] = result;

		//do this for each file
		for (auto ii : result->statements)
		{
			ii->CompileDeclarations(global);
		}
	}

	for (auto result : asts)
	{
		current_source = sources[result.first];

		//then do this for each file
		for (auto ii : result.second->statements)
		{
			//catch any exceptions
			try
			{
				ii->Compile(global);
			}
			catch (...)
			{
				//printf("Exception Compiling Line\n");
				errors++;
			}
		}
	}

	auto init = global->AddFunction("global", &IntType, {});
	init->Return(global->Number(6));
	//todo: put intializers here and have this call main()
	//delete result;

error:
	if (errors > 0)
	{
		printf("Compiling Failed: %d Errors Found\n", errors);
		delete module;
		module = 0;
	}

	for (auto ii : sources)
		delete ii.second;

	for (auto ii : asts)
		delete ii.second;

	if (errors == 0)
	{
		llvm::InitializeNativeTarget();
		llvm::InitializeNativeTargetAsmParser();
		llvm::InitializeNativeTargetAsmPrinter();
		//llvm::InitializeNativeTargetAsmParser();
		auto mod = JITHelper->getModuleForNewFunction();
		void* res = JITHelper->getPointerToFunction(this->functions["main"]->f);
		//try and jit
		//llvm::EngineBuilder(this-)
		//go through global scope
		int(*FP)() = (int(*)())(intptr_t)res;
		FP();
	}

	//restore working directory
	chdir(olddir);
	return;
}

void Compiler::Compile(const char* code, const char* filename)
{
//	JITHelper = new MCJITHelper(this->context);
//
//	//spin off children and lets compile this!
//	module = JITHelper->getModuleForNewFunction();// new llvm::Module(filename, context);
//
//	Lexer lexer = Lexer(code, filename);
//	Parser parser = Parser(&lexer);
//
//	//printf("In: %s\n\nResult:\n", code);
//	//result->print();
//	//printf("\n\n");
//	//compile it
//	//first lets create the global context!!
//	//ok this will be the main entry point it initializes everything, then calls the program's entry point
//	int errors = 0;
//	auto global = new CompilerContext(this);
//
//	BlockExpression* result = 0;
//	try
//	{
//		result = parser.parseAll();
//	}
//	catch (...)
//	{
//		printf("Compilation Stopped, Parser Error\n");
//		errors = 1;
//		goto error;
//	}
//
//	//do this for each file
//	for (auto ii : result->statements)
//	{
//		ii->CompileDeclarations(global);
//	}
//
//	//then do this for each file
//	for (auto ii : result->statements)
//	{
//		//catch any exceptions
//		try
//		{
//			ii->Compile(global);
//		}
//		catch (...)
//		{
//			printf("Exception Compiling Line\n");
//			errors++;
//		}
//	}
//
//	auto init = global->AddFunction("global", &IntType, {});
//	init->Return(global->Number(6));
//	//todo: put intializers here and have this call main()
//	delete result;
//
//error:
//	if (errors > 0)
//	{
//		printf("Compiling Failed: %d Errors Found\n", errors);
//		delete module;
//		module = 0;
//	}
//
//	llvm::InitializeNativeTarget();
//	llvm::InitializeNativeTargetAsmParser();
//	llvm::InitializeNativeTargetAsmPrinter();
//	//llvm::InitializeNativeTargetAsmParser();
//	auto mod = JITHelper->getModuleForNewFunction();
//	void* res = JITHelper->getPointerToFunction(this->functions["main"]->f);
//	//try and jit
//	//llvm::EngineBuilder(this-)
//	//go through global scope
//	int(*FP)() = (int(*)())(intptr_t)res;
//	FP();
//	return;
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

CValue CompilerContext::UnaryOperation(TokenType operation, CValue value)
{
	llvm::Value* res = 0;

	if (operation == TokenType::BAnd)
	{
		//this should already have been done elsewhere, so error
		throw 7;
	}

	if (value.type->type == Types::Float || value.type->type == Types::Double)
	{
		switch (operation)
		{
			/*case TokenType::Increment:
				parent->builder.create
				throw 7;
				//res = parent->builder.CreateFAdd(left.val, right.val);
				break;
				case TokenType::Decrement:
				throw 7;
				//res = parent->builder.CreateFSub(left.val, right.val);
				break;*/
		case TokenType::Minus:
			res = parent->builder.CreateFNeg(value.val);// parent->builder.CreateFMul(left.val, right.val);
			break;
		default:
			Error("Invalid Unary Operation '" + TokenToString[operation] + "' On Type '" + value.type->ToString() + "'", *current_token);
			break;
		}

		return CValue(value.type, res);
	}
	else if (value.type->type == Types::Int || value.type->type == Types::Short || value.type->type == Types::Char)
	{
		//integer probably
		switch (operation)
		{
		case TokenType::Increment:
			res = parent->builder.CreateAdd(value.val, parent->builder.getInt32(1));
			break;
		case TokenType::Decrement:
			res = parent->builder.CreateSub(value.val, parent->builder.getInt32(1));
			break;
		case TokenType::Minus:
			res = parent->builder.CreateNeg(value.val);// parent->builder.CreateFMul(left.val, right.val);
			break;
		case TokenType::BNot:
			res = parent->builder.CreateNot(value.val);
			break;
		default:
			Error("Invalid Unary Operation '" + TokenToString[operation] + "' On Type '" + value.type->ToString() + "'", *current_token);
			break;
		}

		return CValue(value.type, res);
	}
	else if (value.type->type == Types::Pointer)
	{
		switch (operation)
		{
		case TokenType::Asterisk:
			return CValue(value.type->base, this->parent->builder.CreateLoad(value.val));
		default:
			Error("Invalid Unary Operation '" + TokenToString[operation] + "' On Type '" + value.type->ToString() + "'", *current_token);
		}

	}
	//throw 7;
	Error("Invalid Unary Operation '" + TokenToString[operation] + "' On Type '" + value.type->ToString() + "'", *current_token);
}

CValue CompilerContext::BinaryOperation(Jet::TokenType op, CValue left, CValue right)
{
	llvm::Value* res = 0;

	//should this be a floating point calc?
	if (left.type->type != right.type->type)
	{
		//conversion time!!

		throw 7;
	}

	if (left.type->type == Types::Float || left.type->type == Types::Double)
	{
		switch (op)
		{
		case TokenType::AddAssign:
		case TokenType::Plus:
			res = parent->builder.CreateFAdd(left.val, right.val);
			break;
		case TokenType::SubtractAssign:
		case TokenType::Minus:
			res = parent->builder.CreateFSub(left.val, right.val);
			break;
		case TokenType::MultiplyAssign:
		case TokenType::Asterisk:
			res = parent->builder.CreateFMul(left.val, right.val);
			break;
		case TokenType::DivideAssign:
		case TokenType::Slash:
			res = parent->builder.CreateFDiv(left.val, right.val);
			break;
		case TokenType::LessThan:
			//use U or O?
			res = parent->builder.CreateFCmpULT(left.val, right.val);
			return CValue(&BoolType, res);
			break;
		case TokenType::LessThanEqual:
			res = parent->builder.CreateFCmpULE(left.val, right.val);
			return CValue(&BoolType, res);
			break;
		case TokenType::GreaterThan:
			res = parent->builder.CreateFCmpUGT(left.val, right.val);
			return CValue(&BoolType, res);
			break;
		case TokenType::GreaterThanEqual:
			res = parent->builder.CreateFCmpUGE(left.val, right.val);
			return CValue(&BoolType, res);
			break;
		case TokenType::Equals:
			res = parent->builder.CreateFCmpUEQ(left.val, right.val);
			return CValue(&BoolType, res);
			break;
		case TokenType::NotEqual:
			res = parent->builder.CreateFCmpUNE(left.val, right.val);
			return CValue(&BoolType, res);
			break;
		default:
			Error("Invalid Binary Operation '" + TokenToString[op] + "' On Type '" + left.type->ToString() + "'", *current_token);
			//throw 7;
			break;
		}

		return CValue(left.type, res);
	}
	else if (left.type->type == Types::Int || left.type->type == Types::Short || left.type->type == Types::Char)
	{
		//integer probably
		switch (op)
		{
		case TokenType::AddAssign:
		case TokenType::Plus:
			res = parent->builder.CreateAdd(left.val, right.val);
			break;
		case TokenType::SubtractAssign:
		case TokenType::Minus:
			res = parent->builder.CreateSub(left.val, right.val);
			break;
		case TokenType::MultiplyAssign:
		case TokenType::Asterisk:
			res = parent->builder.CreateMul(left.val, right.val);
			break;
		case TokenType::DivideAssign:
		case TokenType::Slash:
			if (true)//signed
				res = parent->builder.CreateSDiv(left.val, right.val);
			else//unsigned
				res = parent->builder.CreateUDiv(left.val, right.val);
			break;
		case TokenType::Modulo:
			if (true)//signed
				res = parent->builder.CreateSRem(left.val, right.val);
			else//unsigned
				res = parent->builder.CreateURem(left.val, right.val);
			break;
			//todo add unsigned
		case TokenType::LessThan:
			//use U or S?
			res = parent->builder.CreateICmpSLT(left.val, right.val);
			return CValue(&BoolType, res);
			break;
		case TokenType::LessThanEqual:
			res = parent->builder.CreateICmpSLE(left.val, right.val);
			return CValue(&BoolType, res);
			break;
		case TokenType::GreaterThan:
			res = parent->builder.CreateICmpSGT(left.val, right.val);
			return CValue(&BoolType, res);
			break;
		case TokenType::GreaterThanEqual:
			res = parent->builder.CreateICmpSGE(left.val, right.val);
			return CValue(&BoolType, res);
			break;
		case TokenType::Equals:
			res = parent->builder.CreateICmpEQ(left.val, right.val);
			return CValue(&BoolType, res);
			break;
		case TokenType::NotEqual:
			res = parent->builder.CreateICmpNE(left.val, right.val);
			return CValue(&BoolType, res);
			break;
		case TokenType::BAnd:
		case TokenType::AndAssign:
			res = parent->builder.CreateAnd(left.val, right.val);
			break;
		case TokenType::BOr:
		case TokenType::OrAssign:
			res = parent->builder.CreateOr(left.val, right.val);
			break;
		case TokenType::Xor:
		case TokenType::XorAssign:
			res = parent->builder.CreateXor(left.val, right.val);
			break;
		case TokenType::LeftShift:
			//todo
		case TokenType::RightShift:
			//todo
		default:
			Error("Invalid Binary Operation '" + TokenToString[op] + "' On Type '" + left.type->ToString() + "'", *current_token);
			//throw 7;
			break;
		}

		return CValue(left.type, res);
	}
	//printf("Invalid Binary Operation!\n");

	Error("Invalid Binary Operation '" + TokenToString[op] + "' On Type '" + left.type->ToString() + "'", *current_token);
	//throw 7;
	//return res;
}