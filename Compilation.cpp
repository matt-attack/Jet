#include "Compiler.h"
#include "Project.h"
#include "Source.h"
#include "Expressions.h"
#include "Lexer.h"
#include "Types/Function.h"

#include <direct.h>

#include <llvm-c\Core.h>
#include <llvm/ADT/Triple.h>
#include <llvm/Support/Host.h>
#include <llvm/Analysis/TargetLibraryInfo.h>
#include <llvm/IR/AssemblyAnnotationWriter.h>
#include <llvm/Support/FormattedStream.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/CodeGen/CommandFlags.h>
#include <llvm/Target/TargetRegisterInfo.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetSubtargetInfo.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/GlobalVariable.h>

#ifdef _WIN32
#include <Windows.h>
#endif

using namespace Jet;

//this is VERY TERRIBLE remove later
Source* current_source = 0;

//options for the linker
#ifdef _WIN32
#define USE_MSVC
#else
#define USE_GCC
#endif

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


Compilation::Compilation(JetProject* proj) : builder(llvm::getGlobalContext()), context(llvm::getGlobalContext()), project(proj)
{
	this->target = 0;
	this->ns = new Namespace;
	this->ns->parent = 0;
	this->global = this->ns;

	//insert basic types
	ns->members.insert({ "float", new Type("float", Types::Float) });
	this->DoubleType = new Type("double", Types::Double);
	ns->members.insert({ "double", this->DoubleType }); //types["double"] = this->DoubleType;// new Type("double", Types::Double);// &DoubleType;// new Type(Types::Float);
	ns->members.insert({ "long", new Type("long", Types::Long) }); //types["long"] = new Type("long", Types::Long);
	this->IntType = new Type("int", Types::Int);
	ns->members.insert({ "int", this->IntType });//types["int"] = this->IntType;// new Type("int", Types::Int);// &IntType;// new Type(Types::Int);
	ns->members.insert({ "short", new Type("short", Types::Short) });//types["short"] = new Type("short", Types::Short);
	ns->members.insert({ "char", new Type("char", Types::Char) });//types["char"] = new Type("char", Types::Char);
	this->BoolType = new Type("bool", Types::Bool);// &BoolType;// new Type(Types::Bool);
	ns->members.insert({ "bool", this->BoolType });// types["bool"] = this->BoolType;
	ns->members.insert({ "void", new Type("void", Types::Void) });// types["void"] = new Type("void", Types::Void);// &VoidType;// new Type(Types::Void);
}

Compilation::~Compilation()
{
	//free global namespace
	delete this->global;

	//free functions
	for (auto ii : this->functions)
		delete ii;

	//free ASTs
	for (auto ii : asts)
	{
		//std::string out;

		//if (errors == 0)
		//{
		//MemberRenamer renamer("string", "length", "apples", this);
		//ii.second->Visit(&renamer);
		//ii.second->Print(out, sources[ii.first]);
		//}
		//printf("%s",out.c_str());

		delete ii.second;
	}

	//free sources
	for (auto ii : sources)
		delete ii.second;

	//free function types
	for (auto ii : function_types)
		delete ii.second;

	//free traits
	for (auto ii : this->traits)
		delete ii.second;
}

class StackTime
{
public:
	long long start;
	long long rate;
	char* name;

	StackTime(char* name);

	~StackTime();
};

StackTime::StackTime(char* name)
{
	this->name = name;

#ifndef _WIN32
	start = gettime2();
	rate = 1000000;
#else
	QueryPerformanceCounter((LARGE_INTEGER *)&start);
	QueryPerformanceFrequency((LARGE_INTEGER *)&rate);
#endif
}
StackTime::~StackTime()
{
	long long  end;
#ifdef _WIN32
	QueryPerformanceCounter((LARGE_INTEGER *)&end);
#else
	end = gettime2();
#endif

	long long diff = end - start;
	float dt = ((double)diff) / ((double)rate);
	printf("%s Time: %f seconds\n", this->name, dt);
}



Compilation* Compilation::Make(JetProject* project)
{
	Compilation* compilation = new Compilation(project);

	char olddir[500];
	getcwd(olddir, 500);
	std::string path = project->path;
	path += '/';

	chdir(path.c_str());

	std::vector<char*> lib_symbols;
	int deps = project->dependencies.size();
	for (int i = 0; i < deps; i++)
	{
		auto ii = project->dependencies[i];

		//read in declarations for each dependency
		std::string symbol_filepath = ii + "/build/symbols.jlib";
		std::ifstream symbols(symbol_filepath, std::ios_base::binary);
		if (symbols.is_open() == false)
		{
			for (auto ii : lib_symbols)
				delete[] ii;

			printf("Dependency compilation failed: could not find symbol file!\n");
			return 0;
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
	//JITHelper = new MCJITHelper(this->context);

	//spin off children and lets compile this!

	//module = JITHelper->getModuleForNewFunction();
	compilation->module = new llvm::Module("hi.im.jet", compilation->context);
	//compilation->module->

	compilation->debug = new llvm::DIBuilder(*compilation->module, true);
	compilation->debug_info.cu = compilation->debug->createCompileUnit(dwarf::DW_LANG_C, "../aaaa.jet", ".", "Jet Compiler", false, "", 0, "");

	//compile it
	//first lets create the global context!!
	//ok this will be the main entry point it initializes everything, then calls the program's entry point
	int errors = 0;
	auto global = new CompilerContext(compilation);
	compilation->current_function = global;
	compilation->sources = project->GetSources();

	//read in symbols from lib
	std::vector<BlockExpression*> symbol_asts;
	std::vector<Source*> symbol_sources;
	{
		StackTime timer("Reading Symbols");
		for (auto buffer : lib_symbols)
		{
			Source* src = new Source(buffer, "symbols");

			BlockExpression* result = 0;
			try
			{
				result = src->GetAST();
				result->CompileDeclarations(global);
				symbol_asts.push_back(result);
				symbol_sources.push_back(src);
			}
			catch (...)
			{
				delete result;
				printf("Compilation Stopped, Error Parsing Symbols\n");
				delete compilation;
				compilation = 0;
				errors = 1;
				goto error;
			}

			compilation->asts["symbols_" + std::to_string(symbol_asts.size())] = result;
			compilation->sources["symbols_" + std::to_string(symbol_asts.size())] = src;

			//this fixes some errors, need to resolve them later
			compilation->debug_info.file = compilation->debug->createFile("temp",
				compilation->debug_info.cu->getDirectory());

			compilation->ResolveTypes();
		}
	}

	//read in each file
	//these two blocks could be multithreaded! theoretically
	{
		StackTime timer("Parsing Files and Compiling Declarations");

		for (auto file : compilation->sources)
		{
			if (file.second == 0)
			{
				printf("Could not find file '%s'!\n", file.first.c_str());
				errors = 1;
				goto error;
			}

			BlockExpression* result = 0;
			try
			{
				result = file.second->GetAST();
			}
			catch (...)
			{
				printf("Compilation Stopped, Parser Error\n");
				errors = 1;
				delete compilation;
				compilation = 0;
				goto error;
			}
			compilation->asts[file.first] = result;

			compilation->current_function = global;

			//do this for each file
			for (auto ii : result->statements)
			{
				try
				{
					ii->CompileDeclarations(global);//guaranteed not to throw?
				}
				catch (int x)
				{
					errors++;

					goto error;
				}
			}
		}
	}

	//this fixes some errors, need to resolve them later
	compilation->debug_info.file = compilation->debug->createFile("temp",
		compilation->debug_info.cu->getDirectory());

	try
	{
		StackTime tt("Resolving Types");
		compilation->ResolveTypes();
	}
	catch (int x)
	{
		errors++;
		goto error;
	}

	//load all types
	//for (auto ii : this->types)
	//if (!(ii.second->type == Types::Struct && ii.second->data->templates.size()))
	//	ii.second->Load(this);

	{
		StackTime timer("Final Compiler Pass");

		for (auto result : compilation->asts)
		{
			current_source = compilation->sources[result.first];
			compilation->current_function = global;

			//ok, I only need one of these, fixme
			//debug_info.cu = debug->createCompileUnit(dwarf::DW_LANG_C, result.first, "../", "Jet Compiler", false, "", 0, "");
			compilation->debug_info.file = compilation->debug->createFile(result.first,
				compilation->debug_info.cu->getDirectory());
			compilation->builder.SetCurrentDebugLocation(llvm::DebugLoc::get(0, 0, compilation->debug_info.file));

			//make sure to set the file name differently for different files
			//then do this for each file
			for (auto ii : result.second->statements)
			{
				//catch any exceptions
				try
				{
					//assert(compilation->ns == compilation->global);
					ii->Compile(global);
					//assert(compilation->ns == compilation->global);
				}
				catch (...)
				{
					compilation->ns = compilation->global;
					errors++;
				}

				compilation->ns = compilation->global;
			}
		}
	}

	//figure out how to get me working with multiple definitions
	auto init = global->AddFunction("_jet_initializer", compilation->ns->members.find("int")->second.ty, {});
	if (project->IsExecutable())
	{
		//this->builder.SetCurrentDebugLocation(llvm::DebugLoc::get(0, 0, init->function->scope.get()));
		//init->Call("puts", { init->String("hello from initializer") });

		//todo: put intializers here
		init->Call("main", {});
	}
	init->Return(global->Integer(0));

	//compilation->module->dump();
error:

	//restore working directory
	chdir(olddir);
	return compilation;
}

void Compilation::Assemble(int olevel)
{
	if (this->errors.size() > 0)
		return;

	StackTime timer("Assembling Output");

	char olddir[500];
	getcwd(olddir, 500);
	std::string path = project->path;
	path += '/';

	chdir(path.c_str());

	//make the output folder
	mkdir("build/");

	//set target
	this->SetTarget();

	if (olevel > 0)
		this->Optimize(olevel);

	//figure out why the debug fails on larger files 
	//add more debug location emits
	debug->finalize();

	//output the .o file for this package
	this->OutputPackage(project->project_name, olevel);

	//output the IR for debugging
	this->OutputIR("build/output.ir");



	//then, if and only if I am an executable, make the .exe
	if (project->IsExecutable())
	{
		printf("Compiling Executable...\n");
#ifdef USE_GCC
		std::string cmd = "gcc -L. -g ";//-e_jet_initializer

		cmd += "build/" + project->project_name + ".o ";
		cmd += "-o build/" + project->project_name + ".exe ";

		//need to link each dependency
		for (auto ii : project->dependencies)
		{
			cmd += "-L" + ii + "/build/ ";

			cmd += "-l" + GetNameFromPath(ii) + " ";
		}

		cmd += " -L.";
		for (auto ii : project->libs)
			cmd += " -l:\"" + ii + "\" ";
#else
		std::string cmd = "link.exe /ENTRY:_jet_initializer /DEBUG /INCREMENTAL:NO /NOLOGO ";

		cmd += "build/" + project->project_name + ".o ";
		cmd += "/OUT:build/" + project->project_name + ".exe ";

		//need to link each dependency
		for (auto ii : project->dependencies)
			cmd += ii + "/build/lib" + GetNameFromPath(ii) + ".a ";

		for (auto ii : project->libs)
			cmd += " \"" + ii + "\"";
#endif

		auto res = exec(cmd.c_str());
		printf(res.c_str());
	}
	else
	{
		std::vector<std::string> temps;

		//need to put this stuff in the .jlib file later
		printf("Compiling Lib...\n");

#ifndef USE_GCC
		std::string ar = "llvm-ar";
#else
		std::string ar = "ar";
#endif

		std::string cmd = ar + " rcs build/lib" + project->project_name + ".a ";
		cmd += "build/" + project->project_name + ".o ";

		for (auto ii : project->dependencies)
		{
			//need to extract then merge
			//can use llvm-ar or just ar
			std::string cm = ar + " x " + ii + "/build/lib" + GetNameFromPath(ii) + ".a";
			auto res = exec(cm.c_str());
			printf(res.c_str());

			//get a list of the extracted files
			cm = ar + " t " + ii + "/build/lib" + GetNameFromPath(ii) + ".a";
			res = exec(cm.c_str());
			int i = 0;
			while (true)
			{
				std::string file;
				if (i >= res.length())
					break;

				while (res[i] != '\n')
					file += res[i++];
				i++;

				temps.push_back(file);
				cmd += file + " ";
			}
		}

		auto res = exec(cmd.c_str());
		printf(res.c_str());

		//delete temporary files
		for (auto ii : temps)
			DeleteFile(ii.c_str());
	}

	//restore working directory
	chdir(olddir);
}

void Compilation::Optimize(int level)
{
	llvm::legacy::FunctionPassManager OurFPM(module);
	// Set up the optimizer pipeline.  Start with registering info about how the
	// target lays out data structures.
	//TheModule->setDataLayout(*TheExecutionEngine->getDataLayout());
	// Do the main datalayout
	//OurFPM.add(new llvm::DataLayoutPass());
	// Provide basic AliasAnalysis support for GVN.
	OurFPM.add(llvm::createBasicAliasAnalysisPass());
	// Do simple "peephole" optimizations and bit-twiddling optzns.
	OurFPM.add(llvm::createInstructionCombiningPass());
	// Reassociate expressions.
	OurFPM.add(llvm::createReassociatePass());
	// Promote allocas to registers
	OurFPM.add(llvm::createPromoteMemoryToRegisterPass());
	// Eliminate Common SubExpressions.
	OurFPM.add(llvm::createGVNPass());
	// Simplify the control flow graph (deleting unreachable blocks, etc).
	OurFPM.add(llvm::createCFGSimplificationPass());


	if (level > 1)
	{
		OurFPM.add(llvm::createDeadCodeEliminationPass());
	}

	OurFPM.doInitialization();

	//run it on all functions
	for (auto fun : this->functions)
	{
		if (fun && fun->f && fun->expression)
		{
			OurFPM.run(*fun->f);
		}
	}
}

void Compilation::SetTarget()
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
	this->target = TheTarget->createTargetMachine(TheTriple.getTriple(), MCPU, FeaturesStr, Options, RelocModel, CodeModel, OLvl);

	module->setDataLayout(*this->target->getDataLayout());
}

void Compilation::OutputPackage(const std::string& project_name, int o_level)
{
	llvm::legacy::PassManager MPM;

	std::error_code ec;
	llvm::raw_fd_ostream strr("build/" + project_name + ".o", ec, llvm::sys::fs::OpenFlags::F_None);
	llvm::formatted_raw_ostream oo(strr);
	llvm::AssemblyAnnotationWriter writer;

	//no idea what this does LOL
	//llvm::TargetLibraryInfo *TLI = new llvm::TargetLibraryInfo::;
	//if (true)
	//TLI->disableAllFunctions();
	//MPM.add(TLI);
	if (o_level > 0)
	{
		MPM.add(llvm::createFunctionInliningPass(o_level, 3));
	}
	//MPM.add(new llvm::DataLayoutPass());
	target->addPassesToEmitFile(MPM, strr, llvm::TargetMachine::CodeGenFileType::CGFT_ObjectFile, false);

	//std::error_code ecc;
	//llvm::raw_fd_ostream strrr("build/" + project_name + ".s", ecc, llvm::sys::fs::OpenFlags::F_None);
	//llvm::formatted_raw_ostream oo2(strrr);

	//Target->addPassesToEmitFile(MPM, oo2, llvm::TargetMachine::CodeGenFileType::CGFT_AssemblyFile, false);

	MPM.run(*module);

	//auto mod = JITHelper->getModuleForNewFunction();
	//void* res = JITHelper->getPointerToFunction(this->functions["main"]->f);
	//try and jit
	//go through global scope
	//int(*FP)() = (int(*)())(intptr_t)res;
	//FP();
	//add runtime option to switch between gcc and link
	//build symbol table for export
	printf("Building Symbol Table...\n");
	StackTime timer("Writing Symbol Output");
	std::string function;
	this->ns->OutputMetadata(function, this);

	//todo: only do this if im a library
	
	std::ofstream stable("build/symbols.jlib", std::ios_base::binary);
	stable.write(function.data(), function.length());
	stable.close();
}

void Compilation::OutputIR(const char* filename)
{
	std::error_code ec;
	llvm::raw_fd_ostream str(filename, ec, llvm::sys::fs::OpenFlags::F_None);
	llvm::formatted_raw_ostream o(str);
	llvm::AssemblyAnnotationWriter writer;
	module->print(str, &writer);
}

void Compilation::AdvanceTypeLookup(Jet::Type** dest, const std::string& name, Token* location)
{
	//who knows what type it is, create a dummy one that will get replaced later
	Type* type = new Type;
	type->name = name;
	type->type = Types::Invalid;
	type->data = 0;
	type->ns = this->ns;
	type->_location = location;
	types.push_back({ this->ns, dest });

	*dest = type;
}

Jet::Type* Compilation::TryLookupType(const std::string& name)
{
	auto pos = name.find_first_of("::");
	if (pos != -1)
	{
		//navigate to correct namespace
		std::string ns = name.substr(0, pos);
		auto res = this->ns->members.find(ns);
		auto old = this->ns;
		this->ns = res->second.ns;
		auto out = this->TryLookupType(name.substr(pos + 2));
		this->ns = old;

		return out;
	}

	int i = 0;
	while (IsLetter(name[i++])) {};
	std::string base = name.substr(0, i - 1);

	//look through namespaces to find the base type
	auto curns = this->ns;
	auto res = this->ns->members.find(base);
	while (res == curns->members.end())
	{
		curns = curns->parent;
		if (curns == 0)
			break;

		res = curns->members.find(base);
	}

	if (curns)
		res = curns->members.find(name);

	auto type = (curns != 0 && res != curns->members.end() && res->second.type == SymbolType::Type) ? res->second.ty : 0;
	return type;
}


Jet::Type* Compilation::LookupType(const std::string& name, bool load)
{
	int i = 0;
	while (IsLetter(name[i++])) {};

	if (name.length() > i + 1 && name[i - 1] == ':' && name[i] == ':')
	{
		//navigate to correct namespace
		std::string ns = name.substr(0, i - 1);
		auto res = this->ns->members.find(ns);
		if (res == this->ns->members.end())
			this->Error("Namespace " + ns + " not found", *this->current_function->current_token);
		auto old = this->ns;
		this->ns = res->second.ns;
		auto out = this->LookupType(name.substr(i - 1 + 2), load);
		this->ns = old;

		return out;
	}
	std::string base = name.substr(0, i - 1);

	//look through namespaces to find the base type
	auto curns = this->ns;
	auto res = this->ns->members.find(base);
	if (name.back() != ')')
	{
		while (res == curns->members.end())
		{
			curns = curns->parent;
			if (curns == 0)
				break;

			res = curns->members.find(base);
		}
	}
	else
	{
		res = this->ns->members.end();
		curns = this->ns;
	}

	if (curns)
		res = curns->members.find(name);

	auto type = (curns != 0 && res != curns->members.end() && res->second.type == SymbolType::Type) ? res->second.ty : 0;

	if (type == 0)
	{
		//time to handle pointers yo
		if (name.length() == 0)
		{
			Error("Missing type specifier, could not infer type", *this->current_function->current_token);
		}
		else if (name[name.length() - 1] == '*')
		{
			//its a pointer
			auto t = this->LookupType(name.substr(0, name.length() - 1), load);

			if (t->pointer_type)
				return t->pointer_type;

			type = new Type;
			type->name = name;
			type->base = t;
			type->type = Types::Pointer;
			t->pointer_type = type;
			if (load)
				type->Load(this);
			else
				type->ns = type->base->ns;
			type->base->ns->members.insert({ name, type });
		}
		else if (name[name.length() - 1] == ']')
		{
			//its an array
			int p = name.find_first_of('[');

			auto len = name.substr(p + 1, name.length() - p - 2);

			auto tname = name.substr(0, p);
			auto t = this->LookupType(tname, load);

			type = new Type;
			type->name = name;
			type->base = t;
			type->type = Types::Array;
			type->size = std::stoi(len);//cheat for now
			curns->members.insert({ name, type });
		}
		else if (name[name.length() - 1] == '>')
		{
			//its a template
			//get first bit, then we can instatiate it
			int p = name.find_first_of('<');

			std::string base = name.substr(0, p);

			//parse types
			std::vector<Type*> types;
			p++;
			do
			{
				//lets cheat for the moment ok
				std::string subtype = ParseType(name.c_str(), p);

				Type* t = this->LookupType(subtype, load);
				types.push_back(t);
			} while (name[p++] != '>');

			//look up the base, and lets instantiate it
			auto t = this->LookupType(base, false);

			type = t->Instantiate(this, types);
		}
		else if (name[name.length() - 1] == ')')
		{
			//work from back to start
			int p = 0;
			int sl = 0;
			int bl = 0;
			for (p = name.length() - 1; p >= 0; p--)
			{
				switch (name[p])
				{
				case '(':
					bl++;
					break;
				case ')':
					bl--;
					break;
				case '<':
					sl--;
					break;
				case '>':
					sl++;
					break;
				}
				if (sl == 0 && bl == 0)
					break;
			}

			std::string ret_type = name.substr(0, p);
			auto rtype = this->LookupType(ret_type, load);

			std::vector<Type*> args;
			//parse types
			p++;
			while (name[p] != ')')
			{
				//lets cheat for the moment ok
				std::string subtype = ParseType(name.c_str(), p);

				Type* t = this->LookupType(subtype, load);
				args.push_back(t);
				if (name[p] == ',')
					p++;
			}
			curns = global;
			type = this->GetFunctionType(rtype, args);
		}
		else
		{
			Error("Reference To Undefined Type '" + name + "'", *this->current_function->current_token);
		}
	}

	//load it if it hasnt been loaded
	if (load && type->loaded == false)
	{
		type->ns = curns;
		type->Load(this);
		type->loaded = true;
	}
	else if (type->loaded == false)
	{
		type->ns = curns;
	}

	return type;
}

CValue Compilation::AddGlobal(const std::string& name, Jet::Type* t)//, bool Extern = false)
{
	auto global = this->globals.find(name);
	if (global != this->globals.end())
		Error("Global variable '" + name + "' already exists", *this->current_function->current_token);

	//auto cons = this->module->getOrInsertGlobal(name, GetType(value.type));
	auto ng = new llvm::GlobalVariable(*module, t->GetLLVMType(), false, llvm::GlobalValue::LinkageTypes::ExternalLinkage, 0, name);

	this->globals[name] = CValue(t, ng);
	return CValue(t, ng);
}

void Compilation::Error(const std::string& string, Token token)
{
	JetError error;
	error.token = token;
	error.message = string;
	error.line = current_source->GetLine(token.line);
	error.file = current_source->filename;
	this->errors.push_back(error);
	throw 7;
}

::Jet::Type* Compilation::GetFunctionType(::Jet::Type* return_type, const std::vector<::Jet::Type*>& args)
{
	int key = (int)return_type;
	for (auto arg : args)
		key ^= (int)arg;

	auto f = this->function_types.find(key);
	if (f == this->function_types.end())
	{
		auto t = new FunctionType;
		t->args = args;
		t->return_type = return_type;

		auto type = new Type;
		type->function = t;
		type->type = Types::Function;
		type->ns = global;
		type->name = type->ToString();

		global->members.insert({ type->name, type });

		function_types[key] = type;
		return type;
	}
	return f->second;
}

void Compilation::ResolveTypes()
{
	auto oldns = this->ns;
	for (int i = 0; i < this->types.size(); i++)
	{
		auto loc = types[i].second;
		if ((*loc)->type == Types::Invalid)
		{
			//resolve me
			this->current_function->current_token = (*loc)->_location;
			this->ns = types[i].first;

			auto res = this->LookupType((*loc)->name, false);

			delete *loc;//free the temporary

			*loc = res;
		}
	}
	this->types.clear();
	this->ns = oldns;
}

Jet::Function* Compilation::GetFunction(const std::string& name)
{
	auto r = this->ns->members.find(name);
	if (r != this->ns->members.end() && r->second.type == SymbolType::Function)
		return r->second.fn;
	//try lower one
	auto next = this->ns->parent;
	while (next)
	{
		r = next->members.find(name);
		if (r != next->members.end() && r->second.type == SymbolType::Function)
			return r->second.fn;

		next = next->parent;
	}
	return 0;
}

Jet::Function* Compilation::GetFunctionAtPoint(const char* file, int line)
{
	for (auto ii : this->functions)
	{
		if (ii->expression)
		{
			auto block = ii->expression->GetBlock();
			if (block->start.line <= line && block->end.line >= line)
			{
				auto src = block->start.GetSource(this);
				if (src->filename == file)
				{
					printf("found it");
					return ii;
				}
			}
		}
	}

	/*for (auto ty : this->types)
	{
		if (ty.second->type == Types::Struct)
		{
			for (auto ii : ty.second->data->functions)
			{
				if (ii.second->expression)
				{
					auto block = ii.second->expression->GetBlock();
					if (block->start.line <= line && block->end.line >= line)
					{
						auto src = block->start.GetSource(this);
						if (src->filename == file)
						{
							printf("found it");
							return ii.second;
						}
					}
				}
			}
		}
		else if (ty.second->type == Types::Trait)
		{
			for (auto ii : ty.second->trait->extension_methods)
			{
				if (ii.second->expression)
				{
					auto block = ii.second->expression->GetBlock();
					if (block->start.line <= line && block->end.line >= line)
					{
						auto src = block->start.GetSource(this);
						if (src->filename == file)
						{
							printf("found it");
							return ii.second;
						}
					}
				}
			}
		}
	}*/
	return 0;
}