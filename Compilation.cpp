#include "Compiler.h"
#include "Project.h"
#include "Source.h"
#include "Expressions.h"

#include <direct.h>


#include <llvm/ADT/Triple.h>
#include <llvm/Support/Host.h>
#include <llvm/Analysis/TargetLibraryInfo.h>
#include <llvm\IR\AssemblyAnnotationWriter.h>
#include <llvm\Support\FormattedStream.h>
#include <llvm\Support\raw_os_ostream.h>
#include <llvm/CodeGen/CommandFlags.h>
#include <llvm\Target\TargetRegisterInfo.h>
#include <llvm\Support\TargetRegistry.h>
#include <llvm\Target\TargetMachine.h>
#include <llvm\Target\TargetSubtargetInfo.h>
#include <llvm/Transforms/IPO.h>
#include <llvm\IR\DataLayout.h>

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

	//insert basic types
	types["float"] = new Type("float", Types::Float);
	this->DoubleType = new Type("double", Types::Double);
	types["double"] = this->DoubleType;// new Type("double", Types::Double);// &DoubleType;// new Type(Types::Float);
	types["long"] = new Type("long", Types::Long);
	this->IntType = new Type("int", Types::Int);
	types["int"] = this->IntType;// new Type("int", Types::Int);// &IntType;// new Type(Types::Int);
	types["short"] = new Type("short", Types::Short);
	types["char"] = new Type("char", Types::Char);
	this->BoolType = new Type("bool", Types::Bool);// &BoolType;// new Type(Types::Bool);
	types["bool"] = this->BoolType;
	types["void"] = new Type("void", Types::Void);// &VoidType;// new Type(Types::Void);
}

Compilation::~Compilation()
{
	//ok, instantiated template types end up being duplicates, that causes the crash when trying to delete them
	//for (auto ii : this->types)
	//if (ii.second && ii.second->type == Types::Struct)//Types::Void)//add more later
	//delete ii.second;

	for (auto ii : this->functions)
		delete ii.second;

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

	for (auto ii : sources)
		delete ii.second;

	//for (auto ii : this->traits)
	//delete ii.second;
}

#include <llvm\DebugInfo\PDB\PDBContext.h>
#include <llvm\DebugInfo\PDB\PDBSymDumper.h>
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

	compilation->debug = new llvm::DIBuilder(*compilation->module);
	compilation->debug_info.cu = compilation->debug->createCompileUnit(dwarf::DW_LANG_C, "../aaaa.jet", ".", "Jet Compiler", false, "", 0, "");

	//compile it
	//first lets create the global context!!
	//ok this will be the main entry point it initializes everything, then calls the program's entry point
	int errors = 0;
	auto global = new CompilerContext(compilation);

	compilation->sources = project->GetSources();
	
	//read in symbols from lib
	std::vector<Expression*> symbol_asts;
	std::vector<Source*> symbol_sources;
	{
		//StackTime timer("Reading Symbols");
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
		}
	}

	//read in each file
	//these two blocks could be multithreaded! theoretically
	{
		//StackTime timer("Parsing Files and Compiling Declarations");

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
				ii->CompileDeclarations(global);//guaranteed not to throw?
		}
	}

	//load all types
	//for (auto ii : this->types)
	//if (!(ii.second->type == Types::Struct && ii.second->data->templates.size()))
	//	ii.second->Load(this);

	{
		//StackTime timer("Final Compiler Pass");

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
					ii->Compile(global);
				}
				catch (...)
				{
					errors++;
				}
			}
		}
	}

	//figure out how to get me working with multiple definitions
	auto init = global->AddFunction("_jet_initializer", compilation->types["int"], {});
	if (project->IsExecutable())
	{
		//this->builder.SetCurrentDebugLocation(llvm::DebugLoc::get(0, 0, init->function->scope.get()));
		//init->Call("puts", { init->String("hello from initializer") });

		//todo: put intializers here
		init->Call("main", {});
	}
	init->Return(global->Integer(0));

error:

	//delete stuff
	for (auto ii : symbol_asts)
		delete ii;

	for (auto ii : symbol_sources)
		delete ii;

	//restore working directory
	chdir(olddir);
	return compilation;
}

void Compilation::Assemble(int olevel)
{
	if (this->errors.size() > 0)
		return;
	//StackTime timer("Building Output");

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
		std::string cmd = "link.exe /ENTRY:_jet_initializer /DEBUG ";

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
		std::string cmd = "llvm-ar rcs build/lib" + project->project_name + ".a ";
		cmd += "build/" + project->project_name + ".o ";

		for (auto ii : project->dependencies)
		{
			//need to extract then merge
			//can use llvm-ar or just ar
			std::string cm = "llvm-ar x " + ii + "/build/lib" + GetNameFromPath(ii) + ".a";
			auto res = exec(cm.c_str());
			printf(res.c_str());

			//get a list of the extracted files
			cm = "ar t " + ii + "/build/lib" + GetNameFromPath(ii) + ".a";
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

	for (auto ii : this->functions)
	{
		if (ii.second->f)
			OurFPM.run(*ii.second->f);
	}

	//run it on member functions
	for (auto ii : this->types)
	{
		if (ii.second && ii.second->type == Types::Struct)
		{
			for (auto fun : ii.second->data->functions)
			{
				if (fun.second->f)
					OurFPM.run(*fun.second->f);
			}
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

	module->setDataLayout(*this->target->getDataLayout());// getSubtargetImpl(*module->getFunctionList().begin())->getDataLayout());
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
	std::string function;
	for (auto ii : this->functions)
	{
		function += "extern fun " + ii.second->return_type->ToString() + " ";
		function += ii.first + "(";
		bool first = false;
		for (auto arg : ii.second->arguments)
		{
			if (first)
				function += ",";
			else
				first = true;

			function += arg.first->ToString() + " " + arg.second;
		}
		function += ");";
	}

	std::string types;
	for (auto ii : this->types)
	{
		if (ii.second == 0)
			continue;
		if (ii.second->type == Types::Struct)
		{
			if (ii.second->data->template_base)
				continue;//dont bother exporting instantiated templates for now

			//export me
			if (ii.second->data->templates.size() > 0)
			{
				types += "struct " + ii.second->data->name + "<";
				for (int i = 0; i < ii.second->data->templates.size(); i++)
				{
					types += ii.second->data->templates[i].first->name + " ";
					types += ii.second->data->templates[i].second;
					if (i < ii.second->data->templates.size() - 1)
						types += ',';
				}
				types += ">{";
			}
			else
			{
				types += "struct " + ii.second->data->name + "{";
			}
			for (auto var : ii.second->data->members)
			{
				if (var.type == 0 || var.type->type == Types::Invalid)//its a template probably?
				{
					types += var.type_name + " ";
					types += var.name + ";";
				}
				else if (var.type->type == Types::Array)
				{
					types += var.type->base->ToString() + " ";
					types += var.name + "[" + std::to_string(var.type->size) + "];";
				}
				else
				{
					types += var.type->ToString() + " ";
					types += var.name + ";";
				}
			}

			if (ii.second->data->templates.size() > 0 && ii.second->data->template_base == 0)
			{
				//output member functions somehow?
				for (auto fun : ii.second->data->functions)
				{
					if (fun.second->expression == 0)
						continue;
					/*types += "fun " + fun.second->return_type->name + " " + fun.first + "(";
					bool first = false;
					for (int i = 1; i < fun.second->argst.size(); i++)/// auto arg : fun.second->argst)
					{
					if (first)
					function += ",";
					else
					first = true;

					function += fun.second->argst[i].first->ToString() + " " + fun.second->argst[i].second;
					}
					types += ") {}";*/
					std::string source;
					fun.second->expression->Print(source, current_source);
					types += source;
					//printf("%s", source.c_str());
				}
				types += "}";
				continue;
			}
			types += "}";

			//output member functions
			for (auto fun : ii.second->data->functions)
			{
				function += "extern fun " + fun.second->return_type->ToString() + " " + ii.second->data->name + "::";
				function += fun.first + "(";
				bool first = false;
				for (int i = 1; i < fun.second->arguments.size(); i++)
				{
					if (first)
						function += ",";
					else
						first = true;

					function += fun.second->arguments[i].first->ToString() + " " + fun.second->arguments[i].second;
				}
				function += ");";
			}
		}
		if (ii.second->type == Types::Trait)
		{
			types += "trait " + ii.first + "{";
			for (auto fun : ii.second->trait->funcs)
			{
				types += " fun " + fun.second->return_type->ToString() + " " + fun.first + "(";
				bool first = false;
				for (auto arg : fun.second->arguments)
				{
					if (first)
						types += ",";
					else
						first = true;

					types += arg.first->ToString() + " " + arg.second;
				}
				types += ");";
			}
			types += "}";
		}
	}

	//todo: only do this if im a library
	std::ofstream stable("build/symbols.jlib", std::ios_base::binary);
	stable.write(types.data(), types.length());
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

Jet::Type* Compilation::AdvanceTypeLookup(const std::string& name)
{
	auto type = types.find(name);
	if (type == types.end())
	{
		//create it, its not a basic type, todo later
		if (name[name.length() - 1] == '*')
		{
			//its a pointer
			auto t = this->AdvanceTypeLookup(name.substr(0, name.length() - 1));

			Type* type = new Type;
			type->name = name;
			type->base = t;
			type->type = Types::Pointer;

			types[name] = type;
			return type;
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

			Type* type = new Type;
			type->base = t;
			type->name = name;
			type->type = Types::Array;
			type->size = std::stoi(len);//cheat for now
			types[name] = type;
			return type;
		}
		//help im getting type duplication with templates7
		//who knows what type it is, create a dummy one
		Type* type = new Type;
		type->name = name;
		type->type = Types::Invalid;
		type->data = 0;
		types[name] = type;

		return type;
	}
	return type->second;
}

Jet::Type* Compilation::LookupType(const std::string& name)
{
	auto iter = types.find(name);
	auto type = iter != types.end() ? iter->second : 0;

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
			auto t = this->LookupType(name.substr(0, name.length() - 1));

			type = new Type;
			type->name = name;
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
			type->name = name;
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
				std::string subtype = ParseType(name.c_str(), p);

				Type* t = this->LookupType(subtype);
				types.push_back(t);
			} while (name[p++] != '>');

			//look up the base, and lets instantiate it
			auto t = this->types.find(base);
			if (t == this->types.end())
				Error("Reference To Undefined Type '" + base + "'", *this->current_function->current_token);

			Type* res = t->second->Instantiate(this, types);
			//resolve the type name down to basics
			//see if the actual thing already exists
			//auto realname = res->ToString();
			//auto f = this->types.find(realname);
			//if (f != this->types.end())
			//	res = f->second;// printf("already exists");
			//else
			//this->types[realname] = res;

			type = res;
		}
		else if (name[name.length() - 1] == ')')
		{
			//work from back to start
			int p = 0;
			int sl = 0;
			int bl = 0;
			for (p = name.length()-1; p >= 0; p--)
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
			auto rtype = this->LookupType(ret_type);

			std::vector<Type*> args;
			//parse types
			p++;
			while (name[p] != ')')
			{
				//lets cheat for the moment ok
				std::string subtype = ParseType(name.c_str(), p);

				Type* t = this->LookupType(subtype);
				args.push_back(t);
				if (name[p] == ',')
					p++;
			}

			auto t = new FunctionType;
			t->args = args;
			t->return_type = rtype;

			type = new Type;
			type->name = name;
			type->function = t;
			type->type = Types::Function;

			types[name] = type;
		}
		else
		{
			Error("Reference To Undefined Type '" + name + "'", *this->current_function->current_token);
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

#include <llvm\IR\GlobalVariable.h>
CValue Compilation::AddGlobal(const std::string& name, Jet::Type* t)//, bool Extern = false)
{
	auto global = this->globals.find(name);
	if (global != this->globals.end())
		Error("Global variable '" + name + "' already exists", *this->current_function->current_token);

	//auto cons = this->module->getOrInsertGlobal(name, GetType(value.type));
	auto ng = new llvm::GlobalVariable(*module, GetType(t), false, llvm::GlobalValue::LinkageTypes::ExternalLinkage, 0, name);

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