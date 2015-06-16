#include "Compiler.h"
#include "CompilerContext.h"

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
	printf("[error] %s %d:%d to %d:%d: %s\n[error] >>>%s\n[error] >>>%s\n\n", current_source->filename.c_str(), token.line, startrow, token.line, endrow, msg.c_str(), code.c_str(), underline.c_str());
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
	printf("[error] %s %d:%d to %d:%d: %s\n[error] >>>%s\n[error] >>>%s\n\n", current_source->filename.c_str(), token.line, startrow, token.line, endrow, msg.c_str(), code.c_str(), underline.c_str());
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

std::vector<std::string> Compiler::Compile(const char* projectdir)
{
	std::vector<std::string> files;
	std::vector<std::string> dependencies;

	//ok, lets parse the jp file
	std::ifstream pf(std::string(projectdir) + "/project.jp", std::ios::in | std::ios::binary);
	if (pf.is_open() == false)
	{
		printf("Error: Could not find project file %s\n", projectdir);
		return std::vector<std::string>();
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

	if (files.size() == 0)
	{
		//if no files, then just add all.jet files in the directory
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
	int deps = dependencies.size();
	for (int i = 0; i < deps; i++)
	{
		auto ii = dependencies[i];
		//spin up new compiler instance and build it
		Compiler compiler;
		auto subdeps = compiler.Compile(ii.c_str());

		//add the subdependencies to the list
		for (auto d : subdeps)
			dependencies.push_back(d);

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
			return std::vector<std::string>();
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

	printf("\nCompiling Project: %s\n", projectdir);

	//read in buildtimes
	std::vector<std::pair<int, int>> buildtimes;
	std::ifstream rebuild("build/rebuild.txt");
	std::string compiler_version;
	if (rebuild.is_open())
	{
		bool first = true;
		do
		{
			std::string line;
			std::getline(rebuild, line, '\n');
			if (line.length() == 0)
				break;
			int hi, lo;
			sscanf(line.c_str(), "%i,%i", &hi, &lo);

			if (first)
			{
				compiler_version = line;
				first = false;
			}
			else
			{
				buildtimes.push_back({ hi, lo });
			}
		} while (true);
	}


	std::vector<std::pair<int, int>> modifiedtimes;
	auto file = CreateFileA("project.jp", GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, 0, NULL);
	FILETIME create, modified, access;
	GetFileTime(file, &create, &access, &modified);

	CloseHandle(file);
	modifiedtimes.push_back({ modified.dwHighDateTime, modified.dwLowDateTime });
	for (auto ii : files)
	{
		auto file = CreateFileA(ii.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
			OPEN_EXISTING, 0, NULL);
		FILETIME create, modified, access;
		GetFileTime(file, &create, &access, &modified);

		//SYSTEMTIME syst;
		//FileTimeToSystemTime(&modified, &syst);
		CloseHandle(file);
		modifiedtimes.push_back({ modified.dwHighDateTime, modified.dwLowDateTime });
	}


	if (strcmp(__TIME__, compiler_version.c_str()) != 0)//see if the compiler was the same
	{
		//do a rebuild compiler version is different
	}
	else if (modifiedtimes.size() == buildtimes.size())//check if files were modified
	{
		for (int i = 0; i < modifiedtimes.size(); i++)
		{
			if (modifiedtimes[i].first == buildtimes[i].first && modifiedtimes[i].second == buildtimes[i].second)
			{
				if (i == modifiedtimes.size() - 1)
				{
					printf("No Changes Detected, Compiling Skipped\n");

					//restore working directory
					chdir(olddir);
					return dependencies;
				}
			}
		}
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
			printf("Could not find file '%s'!\n", file.c_str());
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
				cmd += ii + "/build/output.o ";
			}
			cmd += "build/output.o ";
			cmd += "-o program.exe";
			auto res = exec(cmd.c_str());
			printf(res.c_str());
		}

		//output build times
		std::ofstream rebuild("build/rebuild.txt");
		//output compiler version
		rebuild.write(__TIME__, strlen(__TIME__));
		rebuild.write("\n", 1);
		for (auto ii : modifiedtimes)
		{
			char str[150];
			int len = sprintf(str, "%i,%i\n", ii.first, ii.second);
			rebuild.write(str, len);
		}

		printf("Project built successfully.\n\n");
	}

	//restore working directory
	chdir(olddir);
	return dependencies;
}

void Compiler::Optimize()
{
	llvm::legacy::FunctionPassManager OurFPM(module);
	// Set up the optimizer pipeline.  Start with registering info about how the
	// target lays out data structures.
	//TheModule->setDataLayout(*TheExecutionEngine->getDataLayout());
	// Provide basic AliasAnalysis support for GVN.
	OurFPM.add(llvm::createBasicAliasAnalysisPass());
	// Do simple "peephole" optimizations and bit-twiddling optzns.
	OurFPM.add(llvm::createInstructionCombiningPass());
	// Reassociate expressions.
	OurFPM.add(llvm::createReassociatePass());
	// Eliminate Common SubExpressions.
	OurFPM.add(llvm::createGVNPass());
	// Simplify the control flow graph (deleting unreachable blocks, etc).
	OurFPM.add(llvm::createCFGSimplificationPass());

	OurFPM.doInitialization();

	for (auto ii : this->functions)
	{
		if (ii.second->f)
			OurFPM.run(*ii.second->f);
	}
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
		if (ii.second->type == Types::Struct)
		{
			//export me
			types += "struct " + ii.second->data->name + "{";
			for (auto var : ii.second->data->members)
			{
				if (var.second->type == Types::Array)
				{
					//todo handle multidimensional arrays later
					types += var.second->base->ToString() + " ";
					types += var.first + "[" + std::to_string(var.second->size) + "];";
				}
				else
				{
					types += var.second->ToString() + " ";
					types += var.first + ";";
				}
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

	//todo: only do this if im a library
	std::ofstream stable("build/symbols.jlib");
	stable.write(types.data(), types.length());
	stable.write(function.data(), function.length());
	stable.close();
}

#include "Expressions.h"
void Compiler::OutputIR(const char* filename)
{
	std::error_code ec;
	llvm::raw_fd_ostream str(filename, ec, llvm::sys::fs::OpenFlags::F_None);
	llvm::formatted_raw_ostream o(str);
	llvm::AssemblyAnnotationWriter writer;
	module->print(str, &writer);
}

Jet::Type* Compiler::LookupType(const std::string& name)
{
	auto type = types[name];
	if (type == 0)
	{
		//time to handle pointers yo
		if (name[name.length() - 1] == '*')
		{
			//its a pointer
			auto t = this->LookupType(name.substr(0, name.length() - 1));

			type = new Type;
			type->base = t;
			type->type = Types::Pointer;

			types[name] = type;
		}
		else if (name[name.length() - 1] == ']')
		{
			//its an array
			int p = 0;
			for (p = 0; p < name.length(); p++)
				if (name[p] == '[')
					break;

			auto len = name.substr(p + 1, name.length() - p - 2);

			auto tname = name.substr(0, p);
			auto t = this->LookupType(tname);

			type = new Type;
			type->base = t;
			type->type = Types::Array;
			type->size = std::stoi(len);//cheat for now
			types[name] = type;
		}
		else if (name[name.length() - 1] == '>')
		{
			//its a template
			//get first bit, then we can instatiate it
			int p = 0;
			for (p = 0; p < name.length(); p++)
				if (name[p] == '<')
					break;

			std::string base = name.substr(0, p);

			//parse types
			std::vector<Type*> types;
			p++;
			do
			{
				//lets cheat for the moment ok
				std::string subtype;
				do
				{
					subtype += name[p];
					p++;
				} while (name[p] != ',' && name[p] != '>');

				Type* t = this->LookupType(subtype);
				types.push_back(t);
			} while (name[p++] != '>');

			//look up the base, and lets instantiate it
			auto t = this->types.find(base);
			if (t == this->types.end())
				Error("Reference To Undefined Type '" + base + "'", *this->current_function->current_token);

			Type* res = t->second->Instantiate(this, types);
			//printf("instantiated template type %s!!!\n", name.c_str());
			this->types[name] = res;

			//compile its functions
			if (res->data->expression->functions && res->data->expression->functions->size() > 0)
			{
				StructExpression* expr = dynamic_cast<StructExpression*>(res->data->expression->functions->back()->Parent);
				auto oldname = expr->name;
				expr->name = res->data->name;

				//store then restore insertion point
				auto rp = this->builder.GetInsertBlock();

				for (auto ii : *res->data->expression->functions)
				{
					ii->CompileDeclarations(this->current_function);
				}

				for (auto fun : *res->data->expression->functions)
				{
					fun->Compile(this->current_function);//the context used may not be proper, but it works
					fun->args->clear();//this clears the arguments stored in each fun created when compiledeclarations is called
				}

				this->builder.SetInsertPoint(rp);
				expr->name = oldname;
			}

			type = res;
		}
		else
		{
			Error("Reference To Undefined Type '" + name + "'", *this->current_function->current_token);
			//printf("Error: Couldn't Find Type: %s\n", name.c_str());
			//throw 7;
		}
	}

	//load it if it hasnt been loaded
	if (type->loaded == false)
	{
		type->Load(this);
		type->loaded = true;
	}

	return type;
}