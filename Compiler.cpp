#include "Compiler.h"

#include "Source.h"
#include "Parser.h"
#include "Lexer.h"

#include <direct.h>

#include <fstream>
#include <filesystem>

using namespace Jet;

//this is VERY TERRIBLE remove later
Source* current_source = 0;

//#include "JIT.h"
//MCJITHelper* JITHelper;

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

void Jet::ParserError(const std::string& msg, Token token)
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

CompilerContext* CompilerContext::AddFunction(const std::string& fname, Type* ret, const std::vector<std::pair<Type*, std::string>>& args, bool member)
{
	auto iter = parent->functions.find(fname);
	Function* func;
	if (iter == parent->functions.end())
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
		if (member == false)
			this->parent->functions[fname] = func;

		llvm::BasicBlock *bb = llvm::BasicBlock::Create(parent->context, "entry", n->f);
		parent->builder.SetInsertPoint(bb);
		return n;
	}
	else
	{
		func = iter->second;
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

			break;
		}

		return CValue(left.type, res);
	}

	Error("Invalid Binary Operation '" + TokenToString[op] + "' On Type '" + left.type->ToString() + "'", *current_token);
}

#include <llvm/ADT/Triple.h>
#include <llvm/Support/Host.h>
#include <llvm\Target\TargetLibraryInfo.h>
#include <llvm\IR\AssemblyAnnotationWriter.h>
#include <llvm\Support\FormattedStream.h>
#include <llvm\Support\raw_os_ostream.h>
#include <llvm/CodeGen/CommandFlags.h>
#include <llvm\Target\TargetRegisterInfo.h>
#include <llvm\Support\TargetRegistry.h>
#include <llvm\Target\TargetMachine.h>
#include <llvm\Target\TargetSubtargetInfo.h>

#include <Windows.h>

std::string exec(const char* cmd) {
	FILE* pipe = _popen(cmd, "r");
	if (!pipe) return "ERROR";
	char buffer[128];
	std::string result = "";
	while (!feof(pipe)) {
		if (fgets(buffer, 128, pipe) != NULL)
			result += buffer;
	}
	_pclose(pipe);
	return result;
}

void Compiler::Compile(const char* projectdir)
{
	std::vector<std::string> files;
	std::vector<std::string> dependencies;

	printf("Compiling Project: %s\n", projectdir);

	//ok, lets parse the jp file
	std::ifstream pf(std::string(projectdir) +"/project.jp", std::ios::in | std::ios::binary);
	if (pf.is_open() == false)
	{
		printf("Error: Could not find project file\n");
		return;
	}

	bool is_executable = true;
	int current_block = 0;
	while (pf.peek() != EOF)
	{
		std::string file;//read in a filename
		while (pf.peek() != ' ' && pf.peek() != EOF && pf.peek() != '\r' && pf.peek() != '\n' && pf.peek() != '\t')
			file += pf.get();

		pf.get();

		if (file == "files:")
		{
			current_block = 1;
			continue;
		}
		else if (file == "requires:")
		{
			current_block = 2;
			continue;
		}
		else if (file == "lib:")
		{
			current_block = 3;
			is_executable = false;
			continue;
		}

		switch (current_block)
		{
		case 1:
			if (file.length() > 0)
				files.push_back(file);
			break;
		case 2:
			if (file.length() > 0)
				dependencies.push_back(file);
			break;//do me later
		case 3:
			break;
		default:
			printf("Malformatted Project File!\n");
		}
	}

	char olddir[500];
	getcwd(olddir, 500);
	std::string path = projectdir;
	path += '/';
	/*int i = path.length();
	for (; i >= 0; i--)
	if (path[i] == '\\' || path[i] == '/')
	break;
	path = path.substr(0, i);*/
	chdir(path.c_str());

	//build each dependency
	std::vector<char*> lib_symbols;
	for (auto ii : dependencies)
	{
		//spin up new compiler instance and build it
		Compiler compiler;
		compiler.Compile(ii.c_str());

		std::string path = ii;
		/*int i = path.length();
		for (; i >= 0; i--)
		if (path[i] == '\\' || path[i] == '/')
		break;
		path = path.substr(0, i);*/

		//read in declarations for each dependency
		std::string symbol_filepath = path + "/build/symbols.jlib";
		std::ifstream symbols(symbol_filepath);
		if (symbols.is_open() == false)
		{
			printf("Dependency compilation failed: could not find symbol file!\n");
			return;
		}

		//parse symbols
		symbols.seekg(0, std::ios::end);    // go to the end
		std::streamoff length = symbols.tellg();           // report location (this is the length)
		symbols.seekg(0, std::ios::beg);    // go back to the beginning
		char* buffer = new char[length + 1];    // allocate memory for a buffer of appropriate dimension
		symbols.read(buffer, length);       // read the whole file into the buffer
		buffer[length] = 0;
		symbols.close();

		lib_symbols.push_back(buffer);
	}

	//check if I need a rebuild
	for (auto ii : files)
	{
		auto file = CreateFileA(ii.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
			OPEN_EXISTING, 0, NULL);
		FILETIME create, modified, access;
		GetFileTime(file, &create, &access, &modified);

		SYSTEMTIME syst;
		FileTimeToSystemTime(&modified, &syst);
		CloseHandle(file);
	}

	//JITHelper = new MCJITHelper(this->context);

	//spin off children and lets compile this!
	//module = JITHelper->getModuleForNewFunction();
	module = new llvm::Module("hi", context);

	//compile it
	//first lets create the global context!!
	//ok this will be the main entry point it initializes everything, then calls the program's entry point
	int errors = 0;
	auto global = new CompilerContext(this);

	std::map<std::string, Source*> sources;
	std::map<std::string, BlockExpression*> asts;

	for (auto buffer : lib_symbols)
	{
		Source src(buffer, "symbols");
		current_source = &src;
		Lexer lexer = Lexer(&src);
		Parser parser = Parser(&lexer);

		BlockExpression* result = 0;
		try
		{
			result = parser.parseAll();
			result->CompileDeclarations(global);
		}
		catch (...)
		{
			printf("Compilation Stopped, Error Parsing Symbols\n");
			errors = 1;
			goto error;
		}
	}

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
			errors = 1;
			goto error;
		}
	}

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

	//figure out how to get me working with multiple definitions
	//auto init = global->AddFunction("global", &IntType, {});
	//init->Return(global->Number(6));
	//todo: put intializers here and have this call main()

error:
	for (auto ii : sources)
		delete ii.second;

	for (auto ii : asts)
		delete ii.second;

	if (errors > 0)
	{
		printf("Compiling Failed: %d Errors Found\n", errors);
		delete module;
		module = 0;
	}
	else
	{
		//make the output folder
		mkdir("build/");
		//try and link and shizzle
		//module->dump();

		//output the IR for debugging
		this->OutputIR("build/output.ir");

		//output the .o file for this package
		this->OutputPackage();

		//then, if and only if I am an executable, make the .exe
		if (is_executable)
		{
			printf("Compiling Executable...\n");
			std::string cmd = "clang ";
			for (auto ii : dependencies)
			{
				//todo: compile in the dependencies of dependencies
				cmd += ii + "/build/output.o ";
			}
			cmd += "build/output.o ";
			cmd += "-o program.exe";
			auto res = exec(cmd.c_str());
			printf(res.c_str());
		}

		printf("Project built successfully.\n\n");
	}

	//restore working directory
	chdir(olddir);
	return;
}

void Compiler::OutputPackage()
{
	llvm::InitializeNativeTarget();
	llvm::InitializeNativeTargetAsmParser();
	llvm::InitializeNativeTargetAsmPrinter();

	auto MCPU = llvm::sys::getHostCPUName();

	llvm::Triple TheTriple;
	if (TheTriple.getTriple().empty())
		TheTriple.setTriple(llvm::sys::getDefaultTargetTriple());

	// Get the target specific parser.
	std::string Error;
	const llvm::Target *TheTarget = llvm::TargetRegistry::lookupTarget(MArch, TheTriple, Error);

	llvm::TargetOptions Options = InitTargetOptionsFromCodeGenFlags();
	//Options.MCOption
	//Options.DisableIntegratedAS = NoIntegratedAssembler;
	//Options.MCOptions.ShowMCEncoding = llvm::ShowMCEncoding;
	//Options.MCOptions.MCUseDwarfDirectory = llvm::EnableDwarfDirectory;
	std::string FeaturesStr;
	llvm::CodeGenOpt::Level OLvl = llvm::CodeGenOpt::Default;
	Options.MCOptions.AsmVerbose = false;// llvm::AsmVerbose;
	//llvm::TargetMachine Target(*(llvm::Target*)TheTarget, TheTriple.getTriple(), MCPU, FeaturesStr, Options);
	auto RelocModel = llvm::Reloc::Default;
	auto CodeModel = llvm::CodeModel::Default;
	auto Target = TheTarget->createTargetMachine(TheTriple.getTriple(), MCPU, FeaturesStr, Options, RelocModel, CodeModel, OLvl);


	module->setDataLayout(Target->getSubtargetImpl()->getDataLayout());

	llvm::legacy::PassManager MPM;
	
	//std::string code = "";
	//llvm::raw_string_ostream str(code);
	std::error_code ec;
	llvm::raw_fd_ostream strr("build/output.o", ec, llvm::sys::fs::OpenFlags::F_None);
	llvm::formatted_raw_ostream oo(strr);
	llvm::AssemblyAnnotationWriter writer;

	llvm::TargetLibraryInfo *TLI = new llvm::TargetLibraryInfo();
	if (true)
		TLI->disableAllFunctions();
	MPM.add(TLI);
	MPM.add(new llvm::DataLayoutPass());
	Target->addPassesToEmitFile(MPM, oo, llvm::TargetMachine::CodeGenFileType::CGFT_ObjectFile, false);

	MPM.run(*module);

	//auto mod = JITHelper->getModuleForNewFunction();
	//void* res = JITHelper->getPointerToFunction(this->functions["main"]->f);
	//try and jit
	//go through global scope
	//int(*FP)() = (int(*)())(intptr_t)res;
	//FP();

	//build symbol table for export
	printf("Building Symbol Table...\n");
	std::string function;
	for (auto ii : this->functions)
	{
		function += "extern fun " + ii.second->return_type->ToString() + " ";
		function += ii.first + "(";
		bool first = false;
		for (auto arg : ii.second->argst)
		{
			if (first)
				function += ",";
			else
				first = true;

			function += arg.first->ToString() + " " + arg.second;
		}
		function += ");";
	}

	//need to add generics
	std::string types;
	for (auto ii : this->types)
	{
		if (ii.second->type == Types::Class)
		{
			//export me
			types += "struct " + ii.second->data->name + "{";
			//fix exporting arrays in structs
			for (auto var : ii.second->data->members)
			{
				types += var.second->ToString() + " ";
				types += var.first + ";";
			}
			types += "}";

			//output member functions
			for (auto fun : ii.second->data->functions)
			{
				function += "extern fun " + fun.second->return_type->ToString() + " " + ii.second->data->name + "::";
				function += fun.first + "(";
				bool first = false;
				for (auto arg : fun.second->argst)
				{
					if (first)
						function += ",";
					else
						first = true;

					function += arg.first->ToString() + " " + arg.second;
				}
				function += ");";
			}
		}
	}

	//output to file
	//printf("%s %s\n", function.c_str(), types.c_str());

	//todo: only do this if im a library
	std::ofstream stable("build/symbols.jlib");
	stable.write(types.data(), types.length());
	stable.write(function.data(), function.length());
	stable.close();
}

void Compiler::OutputIR(const char* filename)
{
	std::error_code ec;
	llvm::raw_fd_ostream str(filename, ec, llvm::sys::fs::OpenFlags::F_None);
	llvm::formatted_raw_ostream o(str);
	llvm::AssemblyAnnotationWriter writer;
	module->print(str, &writer);
}