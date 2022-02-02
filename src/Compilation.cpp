#include "Compiler.h"
#include "Project.h"
#include "Source.h"
#include "expressions/Expressions.h"
#include "expressions/DeclarationExpressions.h"
#include "expressions/ControlExpressions.h"
#include "Lexer.h"
#include "types/Function.h"

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <llvm-c/Core.h>
#include <llvm/ADT/Triple.h>
#include <llvm/Support/Host.h>
#include <llvm/Analysis/TargetLibraryInfo.h>
#include <llvm/IR/AssemblyAnnotationWriter.h>
#include <llvm/Support/FormattedStream.h>
#include <llvm/Support/raw_os_ostream.h>
//#include <llvm/CodeGen/CommandFlags.h>
//#include <llvm/Target/TargetRegisterInfo.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Target/TargetMachine.h>
//#include <llvm/Target/TargetSubtargetInfo.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/GlobalVariable.h>

#ifdef _WIN32
#include <Windows.h>
#endif

/*#ifdef _DEBUG
#ifndef DBG_NEW
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#define new DBG_NEW
#endif
#endif  // _DEBUG*/

using namespace Jet;

//options for the linker
#ifdef _WIN32
#define USE_MSVC
#else
#define USE_GCC
#endif


std::string Jet::exec(const char* cmd, int* error_code) {
#ifdef _WIN32
	FILE* pipe = _popen(cmd, "r");
#else
	FILE* pipe = popen(cmd, "r");
#endif 
	if (!pipe) return "ERROR";
	char buffer[128];
	std::string result = "";
	while (!feof(pipe)) {
		if (fgets(buffer, 128, pipe) != NULL)
			result += buffer;
	}
#ifdef _WIN32
	auto res = _pclose(pipe);
#else
	auto res = pclose(pipe);
#endif 
	if (error_code)
	{
		*error_code = WEXITSTATUS(res);
	}
	return result;
}

Compilation::Compilation(const JetProject* proj) : context(), builder(context), project(proj)
{
	this->typecheck = false;
	this->target = 0;
	this->ns = new Namespace;
	this->ns->parent = 0;
	this->global = this->ns;

	//insert basic types
	this->FloatType = new Type(this, "float", Types::Float);
	ns->members.insert({ "float", this->FloatType });
	this->DoubleType = new Type(this, "double", Types::Double);
	ns->members.insert({ "double", this->DoubleType });
	ns->members.insert({ "long", new Type(this, "long", Types::Long) });
	ns->members.insert({ "ulong", new Type(this, "ulong", Types::ULong) });
	this->IntType = new Type(this, "int", Types::Int);
	ns->members.insert({ "int", this->IntType });
	ns->members.insert({ "uint", new Type(this, "uint", Types::UInt) });
	ns->members.insert({ "short", new Type(this, "short", Types::Short) });
	ns->members.insert({ "ushort", new Type(this, "ushort", Types::UShort) });
	ns->members.insert({ "char", new Type(this, "char", Types::Char) });
	ns->members.insert({ "uchar", new Type(this, "uchar", Types::UChar) });
	this->BoolType = new Type(this, "bool", Types::Bool);
	ns->members.insert({ "bool", this->BoolType });
    this->VoidType = new Type(this, "void", Types::Void);
	ns->members.insert({ "void", this->VoidType });

	for (auto ii : ns->members)
	{
		if (ii.second.type == SymbolType::Type)
			ii.second.ty->ns = ns;
	}
	this->CharPointerType = ns->members.find("char")->second.ty->GetPointerType();
}

Compilation::~Compilation()
{
	//free global namespace
	delete this->global;

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

	//free functions
	for (auto ii : this->functions)
		delete ii;

	//free traits
	for (auto ii : this->traits)
		delete ii.second;

	delete this->target;
	//dont delete this->project we dont have ownership
	delete this->debug;
	delete this->module;
}
#ifndef _WIN32
int64_t gettime2()//returns time in microseconds
{
	static __time_t start;
	timespec time;
	//gettimeofday(&time, 0);
	clock_gettime(CLOCK_MONOTONIC, &time);
	return (int64_t)time.tv_sec*1000000 + time.tv_nsec/1000;
}
#endif
class StackTime
{
	bool enable;
public:
	long long start;
	long long rate;
	const char* name;

	StackTime(const char* name, bool enable = true);

	~StackTime();
};

StackTime::StackTime(const char* name, bool time)
{
	this->name = name;
	this->enable = time;

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

	if (this->enable == false)
		return;

	long long diff = end - start;
	float dt = ((double)diff) / ((double)rate);
	printf("%s Time: %f seconds\n", this->name, dt);
}

class TraitChecker : public ExpressionVisitor
{
	Compilation* compiler;
public:

	TraitChecker(Compilation* compiler) : compiler(compiler)
	{

	}

	virtual void Visit(CallExpression* expr)
	{
		auto fun = dynamic_cast<FunctionExpression*>(expr->parent->parent);
		if (fun == 0)
			return;

		if (auto index = dynamic_cast<IndexExpression*>(expr->left))
		{
			if (auto str = dynamic_cast<StructExpression*>(fun->parent))
			{
				if (str->templates)
				{
					auto trait = compiler->LookupType(str->templates->front().type.text);
					//check if the call fits the trait
					//auto out = index->GetBaseType(this->compiler);
					//printf("hi");
				}
			}
		}
	}
};


extern std::string generate_jet_from_header(const char* header);
Compilation* Compilation::Make(const JetProject* project, DiagnosticBuilder* diagnostics, bool time, int debug, std::string target)
{
	Compilation* compilation = new Compilation(project);
	compilation->diagnostics = diagnostics;
	diagnostics->compilation = compilation;
	char olddir[500];
	getcwd(olddir, 500);
	std::string path = project->path;
	path += '/';

	if (path.length() > 1)
		chdir(path.c_str());

	std::vector<std::pair<std::string, char*>> lib_symbols;
	std::vector<std::string> resolved_deps;
    project->ResolveDependencies(resolved_deps);// todo I'm resolving twos
	int deps = project->dependencies.size();

    // build a list of jlibs to load
    std::vector<std::string> jlibs;
    // add any jlibs from the libraries list
    for (auto& lib: project->libs)
    {
        if (lib.find(".jlib") != std::string::npos)
        {
            jlibs.push_back(lib);
        }
    }

    // add jlibs from dependencies
	for (int i = 0; i < deps; i++)
	{
		auto ii = resolved_deps[i];

		if (resolved_deps[i].length() == 0)
		{
			throw 7;// we are missing a dependency
		}

		//read in declarations for each dependency
		jlibs.push_back(ii + "/build/" + GetNameFromPath(ii) + ".jlib");
    }

    for (auto& symbol_filepath: jlibs)
    {
		std::ifstream symbols(symbol_filepath, std::ios_base::binary);
		if (symbols.is_open() == false)
		{
			for (auto ii : lib_symbols)
				delete[] ii.second;

			diagnostics->Error("Dependency include of '" + symbol_filepath + "' failed: could not find symbol file!", "project.jp");

			//restore working directory
			chdir(olddir);

			return 0;
		}

        // todo add an identifier to the start of the file to prevent anything weird from happening
        // if the extension is wrong

		//parse symbols
		symbols.seekg(0, std::ios::end);    // go to the end
		std::streamoff length = symbols.tellg();           // report location (this is the length)
		symbols.seekg(0, std::ios::beg);    // go back to the beginning
		int start;
		symbols.read((char*)&start, 4);
		symbols.seekg(start + 4, std::ios::beg);
		char* buffer = new char[length + 1 - start - 4];    // allocate memory for a buffer of appropriate dimension
		symbols.read(buffer, length);       // read the whole file into the buffer
		buffer[length - start - 4] = 0;
		symbols.close();

		//ok, need to parse this out into different lines/files

		lib_symbols.push_back({ symbol_filepath, buffer });
	}

	//spin off children and lets compile this!
	compilation->module = new llvm::Module(project->project_name, compilation->context);
	compilation->debug = new llvm::DIBuilder(*compilation->module, true);

    
	//set target
	//add string
	//linux i686-pc-linux-gnu
	//raspbian arm-pc-linux-gnueabif"armv6-linux-gnueabihf"
	compilation->SetTarget(target);

	bool emit_debug = debug > 0;
	auto emission_kind = debug <= 1 ? llvm::DICompileUnit::DebugEmissionKind::LineTablesOnly : llvm::DICompileUnit::DebugEmissionKind::FullDebug;

	char tmp_cwd[500];
	getcwd(tmp_cwd, 500);
	auto file = compilation->debug->createFile("../aaaa.jet", tmp_cwd);
	compilation->debug_info.cu = compilation->debug->createCompileUnit(llvm::dwarf::DW_LANG_C, file, "Jet Compiler", false, "", 0, "", emission_kind, 0, emit_debug);

	//compile it
	//first lets create the global context!!
	//ok this will be the main entry point it initializes everything, then calls the program's entry point
	int errors = 0;
	auto global = new CompilerContext(compilation, 0);
	compilation->current_function = global;
	compilation->sources = project->GetSources();

	//add default defines
	auto defines = project->defines;
	// todo dont hardcode this so we can do cross compilation (eventually)
#ifdef _WIN32
	defines["WINDOWS"] = true;
#else
	defines["LINUX"] = true;
#endif
	if (debug > 1)// project debug info is enabled
		defines["DEBUG"] = true;
	else
		defines["RELEASE"] = true;

	//build converted headers and add their source
	for (const auto& hdr_name : project->headers)
	{
		std::string two = hdr_name;
		std::string outfile = two + ".jet";
		//if the header already exists, dont regenerate
		FILE* hdr = fopen(outfile.c_str(), "r");
		if (hdr == 0)//for now just run it every time
		{
			//fix conversion of attributes for calling convention and fix function pointers
			std::string str;// = generate_jet_from_header(two.c_str());
			if (str.length() == 0)
			{
				diagnostics->Error("Could not find header file '" + two + "' to convert.\n", "project.jp");
				errors++;
			}
			else
			{
				std::ofstream o(two + ".jet");
				o << str;
				o.close();
			}
		}
		else
		{
			fclose(hdr);
		}

		//add the source
		std::ifstream t(outfile, std::ios::in | std::ios::binary);
		if (t)
		{
			t.seekg(0, std::ios::end);    // go to the end
			std::streamoff length = t.tellg();           // report location (this is the length)
			t.seekg(0, std::ios::beg);    // go back to the beginning
			char* buffer = new char[length + 1];    // allocate memory for a buffer of appropriate dimension
			t.read(buffer, length);       // read the whole file into the buffer
			buffer[length] = 0;
			t.close();

			compilation->sources[outfile] = new Source(buffer, outfile);
		}
	}

	//read in symbols from lib
	std::vector<BlockExpression*> symbol_asts;
	std::vector<Source*> symbol_sources;
	{
		StackTime timer("Reading Symbols", time);
		compilation->compiling_includes = true;
		std::map<std::string, std::string*> source_strings;//used so we can reuse them
		for (auto buffer : lib_symbols)
		{
			//parse into sources so we can use them below
			const char* data = buffer.second;
			unsigned int len = strlen(buffer.second);
			std::string current_filename = buffer.first;
			//lets read each line at a time
			int i = 0;
			while (i < len)
			{
				//find end of the line
				int start = i;
				while ( i < len && data[i++] != '\n') { }
				int end = i-1;

				const char* line = &data[start];
				if (end - start > 6 && data[start] == '/' 
					&& data[start + 1] == '/' 
					&& data[start + 2] == '!'
					&& data[start + 3] == '@'
					&& data[start + 4] == '!')
				{
					const char* file_name = &data[start + 5];

					//look for end of file_name
					int p = start+5;
					while (p < len && data[p++] != '@') {}

					current_filename = std::string(file_name, p-(start+6));
				}
				//insert the line into the correct source...
				// use current_filename to look up the source
				auto source = source_strings.find(current_filename);
				if (source == source_strings.end())
				{
					//create new one and add it to the list
					source_strings[current_filename] = new std::string();
					source = source_strings.find(current_filename);
				}

				//ok now insert
				source->second->append(line, end - start + 1);
			}
		}

		for (auto ii : source_strings)
		{
			//copy into sources now that we are done
			char* data = new char[ii.second->size()+1];
			strcpy(data, ii.second->c_str());
			delete ii.second;//free the strings now that we are done with them

			Source* src = new Source(data, ii.first);
			compilation->sources["#symbols_" + std::to_string(symbol_asts.size() + 1)] = src;

			BlockExpression* result = src->GetAST(diagnostics, {});
			if (diagnostics->GetErrors().size())
			{
				delete result;
				printf("Compilation Stopped, Error Parsing Symbols\n");
				delete compilation;
				compilation = 0;
				errors = 1;
				goto error;
			}

			try
			{
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

			compilation->asts["#symbols_" + std::to_string(symbol_asts.size())] = result;

			//this fixes some errors, need to resolve them later
			compilation->debug_info.file = compilation->debug->createFile("temp",
				compilation->debug_info.cu->getDirectory());
		}
		compilation->ResolveTypes();
		compilation->compiling_includes = false;
	}

	//read in each file
	//these two blocks could be multithreaded! theoretically
	{
		StackTime timer("Parsing Files and Compiling Declarations", time);

		for (auto file : compilation->sources)
		{
			if (file.second == 0)
			{
				diagnostics->Error("Could not find file '" + file.first + "'.", "project.jp");
				//printf("Could not find file '%s'!\n", file.first.c_str());
				errors = 1;
				goto error;
			}

			if (file.first[0] == '#')//ignore symbol files, we already parsed them
				continue;

			BlockExpression* result = file.second->GetAST(diagnostics, defines);
			if (diagnostics->GetErrors().size())
			{//stop if we encountered a parsing error
				continue;
			}
			//TraitChecker checker(compilation);
			//result->Visit(&checker);

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

		if (diagnostics->GetErrors().size())
		{//stop if we encountered a parsing error
			printf("Compilation Stopped, Parser Error\n");
			errors = diagnostics->GetErrors().size();
			delete compilation;
			compilation = 0;
			goto error;
		}
	}

	//this fixes some errors, need to resolve them later
	compilation->debug_info.file = compilation->debug->createFile("temp",
		compilation->debug_info.cu->getDirectory());

	try
	{
		StackTime tt("Resolving Types", time);
		compilation->ResolveTypes();
	}
	catch (int x)
	{
		errors++;
		goto error;
	}

	{
		StackTime timer("Final Compiler Pass", time);

		for (auto result : compilation->asts)
		{
			if (result.first[0] == '#')
				continue;

			compilation->current_function = global;

			compilation->debug_info.file = compilation->debug->createFile(result.first,
				compilation->debug_info.cu->getDirectory());
			compilation->builder.SetCurrentDebugLocation(llvm::DebugLoc::get(0, 0, compilation->debug_info.file));

			//make sure to set the file name differently for different files
			//then do this for each file
			for (auto ii : result.second->statements)
			{
				//catch any exceptions
				compilation->typecheck = true;
				try
				{
					ii->TypeCheck(global);
				}
				catch (int x)
				{
					compilation->ns = compilation->global;
					errors++;

					compilation->typecheck = false;
					continue;
				}
				compilation->typecheck = false;
				try
				{
					ii->Compile(global);
				}
				catch (int x)
				{
					compilation->ns = compilation->global;
					errors++;
				}

				compilation->ns = compilation->global;
			}

			//compile any templates that were missed
			for (auto temp : compilation->unfinished_templates)
			{
				temp->FinishCompilingTemplate(compilation);
			}
		}
	}

	// Generate initializers.
	if (project->IsExecutable())
	{
		// Clear the debug location for this autogenerated function
		compilation->builder.SetCurrentDebugLocation(0);

		// Executables need an _init function
		auto func = new Function("_init", false, true);
		func->return_type_ = compilation->VoidType;
		compilation->functions.push_back(func);

		auto n = new CompilerContext(compilation, 0);
		n->function = func;
		func->context_ = n;
		compilation->ns->members.insert({ func->name_, func });

		compilation->current_function = n;
		func->Load(compilation);
		llvm::BasicBlock *bb = llvm::BasicBlock::Create(compilation->context, "entry", func->f_);
		compilation->builder.SetInsertPoint(bb);

		compilation->builder.CreateRetVoid();
	}
	else
	{
		// Each library needs an appropriately named initialization function
	}

error:

	//restore working directory
	chdir(olddir);
	return compilation;
}

char* ReadDependenciesFromSymbols(const char* path, int& size)
{
	auto file = fopen(path, "rb");
	if (file)
	{
		fread(&size, 4, 1, file);
		if (size < 0)
		{
			printf("Invalid dependency symbol file!\n");
			return 0;
		}
		char* data = new char[size];
		fread(data, size, 1, file);
		fclose(file);

		return data;
	}
    size = 0;
	return 0;
}

std::string LinkLibLD(const std::string& path)
{
	int div = path.find_last_of('\\');
	int d2 = path.find_last_of('/');
	if (d2 > div)
		div = d2;
	std::string file = path.substr(div+1);
	std::string lib_name = file;
	//strip off any extension
	if (lib_name.find_last_of('.'))
	{
		lib_name = lib_name.substr(0, lib_name.find_last_of('.'));
	}
	if (lib_name.length() > 3 && lib_name[0] == 'l'
		&& lib_name[1] == 'i'
		&& lib_name[2] == 'b')
	{
		lib_name = lib_name.substr(3);
	}
	std::string folder = path.substr(0, div);
	std::string out;
	if (div > -1)
		out += " -L\"" + folder + "\" ";
	else
		out += " -L. ";
	out += " -l\"" + lib_name + "\" ";
	return out;
}

void Compilation::Assemble(const std::vector<std::string>& resolved_deps, const std::string& linker, int olevel, bool time, bool output_ir)
{
	if (this->diagnostics->GetErrors().size() > 0)
		return;

	StackTime timer("Assembling Output", time);

	char olddir[500];
	getcwd(olddir, 500);
	std::string path = project->path;
	path += '/';

	if (path.length() > 1)
		chdir(path.c_str());

	//make the output folder
#ifndef _WIN32
	mkdir("build/", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#else
	mkdir("build/");
#endif

	// now lets split apart the target to get the arch, OS and stdlib (ignoring the second bit)
	std::string target_arch = real_target.substr(0, real_target.find_first_of('-'));
	std::string target_platform = "linux";// for the moment
	std::string target_stdlib = real_target.substr(real_target.find_last_of('-')+1);

	debug->finalize();

	if (olevel > 0)
		this->Optimize(olevel);

	//output the IR for debugging
	if (output_ir)
		this->OutputIR("build/output.ir");

	//output the .o file and .jlib for this package
	this->OutputPackage(resolved_deps, project->project_name, olevel, time);

	//and handling the arguments as well as function overloads, which is still a BIG problem
	//need name mangling
	
	//then, if and only if I am an executable, make the .exe
	if (project->IsExecutable())
	{
		printf("Compiling Executable...\n");

		std::string used_linker = linker;
#ifdef USE_GCC
		if (linker == "")
			used_linker = "ld";
#else
		if (linker == "")
			used_linker = "link.exe";
#endif
		//working gcc command, use this
		////C:\Users\Matthew\Desktop\VM\AsmVM2\AsmVM\async>ld build/async.o ../jetcore/build
		// /jetcore.o -l:"C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\lib\msvcrt
		// .lib" -l:"C:\Program Files (x86)\Windows Kits\8.1\Lib\winv6.3\um\x86\kernel32.li
		///  b" -o build/async_test.exe --entry _main

		if (used_linker.find("link.exe") == -1)
		{
			std::string cmd = used_linker + " ";

			//set entry
            
			// Set the dynamic linker correctly (for linux)
			std::string stdlib = real_target.substr(real_target.find_last_of('-')+1);
			if (target_stdlib == "musl" && target_platform == "linux")
			{
				cmd += "--dynamic-linker=/lib/ld-musl-" + target_arch + ".so.1 ";
			}
			else if (target_platform == "linux")
			{
				std::string arch = target_arch;
				for (auto& c: arch)
				{
					if (c == '_') c = '-';
				}
				if (target_arch.find("64") == -1)
				{
					cmd += "--dynamic-linker=/lib/ld-" + target_platform + "-" + arch + ".so.3 ";
				}
				else
				{
					cmd += "--dynamic-linker=/lib64/ld-" + target_platform + "-" + arch + ".so.2 ";
				}
			}

			// always link in C for the moment, eventually have a way to avoid this
			if (target_platform == "linux")
			{
				cmd += "-L/lib/" + target_arch + "-" + target_platform + "-" + target_stdlib + " ";
				cmd += "-lc ";
				cmd += "/usr/lib/" + target_arch + "-" + target_platform + "-" + target_stdlib + "/crt1.o ";
				if (target_stdlib == "gnu")
				{
					cmd += "/usr/lib/" + target_arch + "-" + target_platform + "-gnu/libc_nonshared.a ";
				}
			}
			else
			{
				// todo link in C for windows (maybe this works)
				//cmd += " -lc ";
			}

			// for when we dont use C
			//cmd += "--entry _main ";

			cmd += "\"build/" + project->project_name + ".o\" ";
			std::string extension = target_platform == "linux" ? "" : ".exe";
			cmd += "-o \"build/" + project->project_name + extension + "\" ";
			//todo need to make sure to link in deps of deps also fix linking
			//need to link each dependency
			for (auto ii : resolved_deps)
			{
				cmd += "\"" + ii + "/build/";//cmd += "-L" + ii + "/build/ ";

				cmd += GetNameFromPath(ii) + ".o\" ";
			}

			//then for each dependency add libs that it needs to link
			for (auto ii : resolved_deps)
			{
				//open up and read first part of the jlib file
				int size = 0;
				char* data = ReadDependenciesFromSymbols((ii + "/build/" + project->project_name + ".jlib").c_str(), size);

				if (data == 0)
				{
					//probably an error
				}

				int pos = 0;
				while (pos < size)
				{
					const char* file = &data[pos];
					if (file[0])
						cmd += LinkLibLD(file);

					pos += strlen(&data[pos]) + 1;
				}
				delete[] data;
			}

			for (auto ii : project->libs)
				cmd += LinkLibLD(ii);
			//rename _context into this in generators, figure out why passing by value into async doesnt work
			//	implement basic containers
			printf("\n%s\n", cmd.c_str());
			int code;
			auto res = exec(cmd.c_str(), &code);
			printf("%s", res.c_str());

			if (code)
			{
				this->diagnostics->Error("Linking failed.", Token());
				return;
			}
		}
		else
		{
			std::string cmd = "link.exe /DEBUG /INCREMENTAL:NO /NOLOGO ";

			//cmd += "/ENTRY:main ";// "/ENTRY:_jet_initializer ";

			cmd += "build/" + project->project_name + ".o ";
			cmd += "/OUT:build/" + project->project_name + ".exe ";

			//need to link each dependency
			for (auto ii : project->dependencies)
				cmd += ii + "/build/lib" + GetNameFromPath(ii) + ".a ";

			//then for each dependency add libs that it needs to link
			for (auto ii : resolved_deps)
			{
				//open up and read first part of the jlib file
				int size;
				char* data = ReadDependenciesFromSymbols((ii + "/build/" + GetNameFromPath(ii) + ".jlib").c_str(), size);

				if (data == 0)
				{
					//probably an error
				}

				int pos = 0;
				while (pos < size)
				{
					const char* file = &data[pos];
					if (file[0])
						cmd += " \"" + std::string(file) + "\"";

					pos += strlen(&data[pos]) + 1;
				}
				delete[] data;
			}

			for (auto ii : project->libs)
				cmd += " \"" + ii + "\"";


			auto res = exec(cmd.c_str());
			printf("%s", res.c_str());
		}
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
        //cmd += "build/" + project->project_name + ".jlib ";

		for (auto ii : resolved_deps)
		{
			//need to extract then merge
			//can use llvm-ar or just ar
			std::string cm = ar + " x " + ii + "/build/lib" + GetNameFromPath(ii) + ".a";
			auto res = exec(cm.c_str());
			printf("%s", res.c_str());

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
		printf("%s", res.c_str());

		//delete temporary files
		for (auto ii : temps)
		{
#ifndef _WIN32
			remove(ii.c_str());
#else
			DeleteFileA(ii.c_str());
#endif
		}
	}

	//restore working directory
	chdir(olddir);
}

void Compilation::Optimize(int level)
{
	//do inlining
	llvm::legacy::PassManager MPM;
	if (level > 0)
	{
		MPM.add(llvm::createFunctionInliningPass(level, 3, false));
		MPM.run(*module);
	}

	llvm::legacy::FunctionPassManager OurFPM(module);
	// Set up the optimizer pipeline.  Start with registering info about how the
	// target lays out data structures.
	//TheModule->setDataLayout(*TheExecutionEngine->getDataLayout());
	// Do the main datalayout
	//OurFPM.add(new llvm::DataLayoutPass());
	// Provide basic AliasAnalysis support for GVN.
	//OurFPM.add(llvm::createBasicAliasAnalysisPass());
	// Do simple "peephole" optimizations and bit-twiddling optzns.
	//OurFPM.add(llvm::createInstructionCombiningPass());
	// Reassociate expressions.
	OurFPM.add(llvm::createReassociatePass());

	//OurFPM.add(llvm::createInstructionSimplifierPass());

	// Promote allocas to registers
	//OurFPM.add(llvm::createPromoteMemoryToRegisterPass());
	// Eliminate Common SubExpressions.
	//OurFPM.add(llvm::createGVNPass());
	// Simplify the control flow graph (deleting unreachable blocks, etc).
	OurFPM.add(llvm::createCFGSimplificationPass());

	if (level > 1)
		OurFPM.add(llvm::createDeadCodeEliminationPass());

	OurFPM.doInitialization();

	//run it on all functions
	for (auto fun : this->functions)
	{
		if (fun && fun->f_ && fun->expression_)
		{
			OurFPM.run(*fun->f_);
		}
	}
}

//#include <llvm\Support\ARMEHABI.h>
std::string Compilation::SetTarget(const std::string& triple)
{
	llvm::InitializeNativeTarget();
	llvm::InitializeNativeTargetAsmParser();
	llvm::InitializeNativeTargetAsmPrinter();
	//LLVMInitializeAllTargets();
	LLVMInitializeARMTargetInfo();
	LLVMInitializeARMTarget();
	LLVMInitializeARMTargetMC();
	LLVMInitializeARMAsmPrinter();
	LLVMInitializeARMAsmParser();
	//LLVMInitializeMSP430Target();
	//llvm::initializeTarget()

	auto MCPU = llvm::sys::getHostCPUName();

	llvm::Triple TheTriple;
	if (triple.length())
	{
		TheTriple.setTriple(triple);
		MCPU = "";
	}
	else
	{
		if (TheTriple.getTriple().empty())
			TheTriple.setTriple(llvm::sys::getDefaultTargetTriple());
	}

	//printf("Assembling as %s\n", TheTriple.str().c_str());


	//std::error_code ec;
	//llvm::raw_fd_ostream strr("build/targets", ec, llvm::sys::fs::OpenFlags::F_None);
	//llvm::TargetRegistry::printRegisteredTargetsForVersion(strr);

	//ok, now for linux builds...
	//TheTriple = llvm::Triple("i686", "pc", "linux", "gnu");
	// Get the target specific parser.
	std::string Error;
	const llvm::Target *TheTarget = llvm::TargetRegistry::lookupTarget(TheTriple.str(), Error);

	if (TheTarget == 0)
	{
		printf("ERROR: Invalid target string! Using system default.\n");

		TheTriple.setTriple(llvm::sys::getDefaultTargetTriple());

		//ok, now for linux builds...
		//TheTriple = llvm::Triple("i686", "pc", "linux", "gnu");
		// Get the target specific parser.
		std::string Error;
		TheTarget = llvm::TargetRegistry::lookupTarget(TheTriple.str(), Error);
	}
	//for linux builds use i686-pc-linux-gnu

	//ok add linux builds
	llvm::TargetOptions Options;// = llvm::InitTargetOptionsFromCodeGenFlags();
	//Options.MCOption
	//Options.DisableIntegratedAS = NoIntegratedAssembler;
	//Options.MCOptions.ShowMCEncoding = llvm::ShowMCEncoding;
	//Options.MCOptions.MCUseDwarfDirectory = llvm::EnableDwarfDirectory;
	std::string FeaturesStr;
	llvm::CodeGenOpt::Level OLvl = llvm::CodeGenOpt::Default;
	Options.MCOptions.AsmVerbose = false;// llvm::AsmVerbose;
	Options.DebuggerTuning = llvm::DebuggerKind::GDB;
	//llvm::TargetMachine Target(*(llvm::Target*)TheTarget, TheTriple.getTriple(), MCPU, FeaturesStr, Options);
	auto RelocModel = llvm::Reloc::Static;//this could be problematic
	auto CodeModel = llvm::CodeModel::Medium;
	this->target = TheTarget->createTargetMachine(TheTriple.getTriple(), MCPU, FeaturesStr, Options, RelocModel, CodeModel, OLvl);

    auto layout = this->target->createDataLayout();

    // Get the actual pointer size
    pointer_size = layout.getPointerSize();
    //printf("Pointer size %i\n", pointer_size);

	module->setDataLayout(layout);

    real_target = TheTriple.str().c_str();

	return TheTriple.str().c_str();
}

void Compilation::OutputPackage(const std::vector<std::string>& resolved_deps, const std::string& project_name, int o_level, bool time)
{
	llvm::legacy::PassManager MPM;

	std::error_code ec;
	llvm::raw_fd_ostream strr("build/" + project_name + ".o", ec, llvm::sys::fs::OpenFlags::F_None);
	//ok, watch out for unsupported calling conventions, need way to specifiy code for windows/linux/cpu
	//add pass to emit the object file
	target->addPassesToEmitFile(MPM, strr, 0, llvm::TargetMachine::CodeGenFileType::CGFT_ObjectFile, false);

	//std::error_code ecc;
	//llvm::raw_fd_ostream strrr("build/" + project_name + ".s", ecc, llvm::sys::fs::OpenFlags::F_RW);
	//llvm::formatted_raw_ostream oo2(strrr);

	//target->addPassesToEmitFile(MPM, strrr, llvm::TargetMachine::CodeGenFileType::CGFT_AssemblyFile, false);
	MPM.run(*module);

	//auto mod = JITHelper->getModuleForNewFunction();
	//void* res = JITHelper->getPointerToFunction(this->functions["main"]->f);
	//try and jit
	//go through global scope
	//int(*FP)() = (int(*)())(intptr_t)res;
	//FP();

	//add runtime option to switch between gcc and link
	//build symbol table for export
	//printf("Building Symbol Table...\n");
	StackTime timer("Writing Symbol Output", time);

	//write out symbol data
	std::string function;
	this->ns->OutputMetadata(function, this, false);
	this->ns->OutputMetadata(function, this, true);

	//only make jlib file if i'm a library
	if (this->project->IsExecutable())
		return;

	std::ofstream stable("build/" + project->project_name + ".jlib", std::ios_base::binary);
	//write names of libraries to link
	std::string libnames;
	//add libs from dependencies
	for (auto ii : resolved_deps)
	{
		//open up and read first part of the jlib file
		int size;
		char* data = ReadDependenciesFromSymbols((ii + "/build/" + GetNameFromPath(ii) + ".jlib").c_str(), size);

		if (data == 0)
		{
			//probably an error
		}

		int pos = 0;
		while (pos < size)
		{
			const char* file = &data[pos];
			if (file[0])
			{
				libnames += std::string(file);
				libnames += '\0';
			}

			pos += strlen(&data[pos]) + 1;
		}
		delete[] data;
	}

	for (auto ii : this->project->libs)
	{
		libnames += ii;//todo: make absolute path
		libnames += '\0';
	}

	int size = libnames.length() + 1;
	stable.write((char*)&size, 4);
	stable.write(libnames.c_str(), libnames.length() + 1);

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
	Type* type = new Type(this, name, Types::Invalid);
	type->data = 0;
	type->ns = this->ns;
	type->_location = location;
	if (this->typecheck)
		type->pointer_type = (Type*)-7;
	unresolved_types.push_back({ this->ns, dest });

	*dest = type;
}

Jet::Type* Compilation::TryLookupType(const std::string& name)
{
	return this->LookupType(name, false, false);
}

Jet::Type* Compilation::LookupType(const std::string& name, bool load, bool do_error, int start_index, bool first_level)
{
	unsigned int i = 0;
	while (IsLetter(name[i]) || IsNumber(name[i]))
	{
		i++;
	};

	// Handle namespaces recursively
	if (name.length() > i && name[i] == ':' && name[i + 1] == ':')
	{
		//navigate to correct namespace
		std::string ns = name.substr(0, i);
        Namespace* new_ns = 0;

		// Try and find this first namespace by recursively going up
        // If this is not the top level, only check the current namespace
		Namespace* cur_ns = this->ns;
		do
		{
			auto res = cur_ns->members.find(ns);
			if (res != cur_ns->members.end())
			{
				new_ns = res->second.ns;
				break;
			}
			cur_ns = cur_ns->parent;
		}
		while (cur_ns && first_level);

        if (new_ns)
        {
			auto old = this->ns;
			this->ns = new_ns;
			auto out = this->LookupType(name.substr(i + 2), load, do_error, start_index + i + 2, false);
			this->ns = old;

			return out;
		}
		else
		{
			if (do_error)
			{
				// Make a new token for this error
				Token relevant_token = *this->current_function->current_token;
				relevant_token.text = relevant_token.text.substr(0, i);
                relevant_token.column += start_index;
				this->Error("Namespace '" + ns + "' not found", relevant_token);
			}
			else
			{
				return 0;
			}
		}
	}

	std::string base = name.substr(0, i);

	//look through namespaces to find the base type
	auto curns = this->ns;
	auto res = this->ns->members.find(base);
	if (name.back() != ')')
	{
		while (res == curns->members.end() && first_level)
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
	{
		res = curns->members.find(name);
	}

	auto type = (curns != 0 && res != curns->members.end() && res->second.type == SymbolType::Type) ? res->second.ty : 0;

	if (type == 0)
	{
		//time to handle pointers yo
		if (name.length() == 0)
		{
			if (do_error)
				Error("Missing type specifier, could not infer type", *this->current_function->current_token);
			else
				return 0;
		}
		else if (name[name.length() - 1] == '*')
		{
			//its a pointer
			auto t = this->LookupType(name.substr(0, name.length() - 1), load, do_error, start_index);

			if (t->pointer_type)
				return t->pointer_type;

			type = new Type(this, name, Types::Pointer);
			type->base = t;
			t->pointer_type = type;
			if (load)
				type->Load(this);
			else
				type->ns = type->base->ns;
			//go down the tree and make sure the base type is valid, this is a pretty dumb hack to solve a template problem
			//but should be durable and can be fixed later
			//todo try and remove this hack
			if (type->base->IsValid())
				type->base->ns->members.insert({ name, type });
		}
		else if (name[name.length() - 1] == ']')
		{
			//its an array
			int p = name.find_last_of('[');

			auto len = name.substr(p + 1, name.length() - p - 2);

			auto tname = name.substr(0, p);
			auto t = this->LookupType(tname, load, do_error, start_index);

			if (len.length())
			{
				type = this->GetInternalArrayType(t, std::atoi(len.c_str()));
				curns->members.insert({ name, type });
			}
			else
			{
				type = this->GetArrayType(t);
				curns->members.insert({ type->name, type });
			}
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
				int cur_index = start_index + p;
				std::string subtype = ParseType(name.c_str(), p);

				Type* t = this->LookupType(subtype, load, do_error, cur_index);
				types.push_back(t);
			} while (name[p++] != '>');

			//look up the base, and lets instantiate it
			auto t = this->LookupType(base, false, do_error, start_index);

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
			auto rtype = this->LookupType(ret_type, load, do_error, start_index);

			std::vector<Type*> args;
			//parse types
			p++;
			while (name[p] != ')')
			{
				//lets cheat for the moment ok
				int cur_index = start_index + p;
				std::string subtype = ParseType(name.c_str(), p);

				Type* t = this->LookupType(subtype, load, do_error, cur_index);
				args.push_back(t);
				if (name[p] == ',')
					p++;
			}
			curns = global;
			type = this->GetFunctionType(rtype, args);
		}
		else
		{
			if (do_error)
			{
				Token tok = *this->current_function->current_token;
				if (start_index > 0)
				{
					// need to create a special token as we are into a type
					tok.text = tok.text.substr(start_index, name.length());
					tok.column += start_index;
				}
				Error("Reference To Undefined Type '" + name + "'", tok);
			}
			else
				return 0;
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

CValue Compilation::AddGlobal(const std::string& name, Jet::Type* t, int size, llvm::Constant* init, bool intern,
  bool is_const)
{
	auto global = this->ns->members.find(name);
	if (global != this->ns->members.end())
    {
        if (this->ns->name.length())
        {
   		    Error("Global '" + name + "' already exists in namespace '" + this->ns->GetQualifiedName() + "'", *this->current_function->current_token);
        }
        else
        {
		    Error("Global '" + name + "' already exists", *this->current_function->current_token);
        }
    }

	llvm::Constant* initializer;
	llvm::Type* type;
	Type* my_type, *ret_type;
	if (size == 0)
	{
		initializer = init ? init : t->GetDefaultValue(this);
		type = t->GetLLVMType();
		my_type = t;
		ret_type = t;
	}
	else
	{
		auto new_type = this->GetInternalArrayType(t, size);
		initializer = init ? init : new_type->GetDefaultValue(this);
		type = llvm::ArrayType::get(t->GetLLVMType(), size);
		ret_type = my_type = new_type->GetPointerType();
	}
	auto ng = new llvm::GlobalVariable(*module, type, false, intern ? llvm::GlobalValue::LinkageTypes::InternalLinkage : llvm::GlobalValue::LinkageTypes::WeakODRLinkage/*ExternalLinkage*/, initializer, name);

	this->debug->createGlobalVariableExpression(this->debug_info.file, name, name, this->debug_info.file, this->current_function->current_token->line, t->GetDebugType(this), !intern);
    if (is_const && size == 0)
    {
        // If its const, we can mark it as a value type
	    this->ns->members.insert({ name, Symbol(new CValue(my_type, initializer, 0), *this->current_function->current_token) });
    }
    else
    {
        this->ns->members.insert({ name, Symbol(new CValue(my_type, 0, ng), *this->current_function->current_token) });
    }

	//todo if it has a constructor, make sure to call it in the initializers...
	return CValue(ret_type, 0, ng);
}

void Compilation::Info(const std::string& string, Token token)
{
	this->diagnostics->Error(string, token, INFO);

	throw 7;
}

void Compilation::Error(const std::string& string, Token token)
{
	this->diagnostics->Error(string, token);

	throw 7;
}

void Compilation::Error(const std::string& string, const std::pair<Token, Token>& tokens)
{
    if (!tokens.second)
    {
        this->diagnostics->Error(string, tokens.first);
    }
    else
    {
        this->diagnostics->Error(string, tokens.first, tokens.second);
    }
    throw 7;
}

void Compilation::Error(const std::string& string, const Token& start, const Token& end)
{
	this->diagnostics->Error(string, start, end);

	throw 7;
}

Jet::Type* Compilation::GetArrayType(Jet::Type* base)
{
	auto res = this->array_types.find(base);
	if (res != this->array_types.end())
		return res->second;

	Type* t = new Type(this, base->name + "[]", Types::Array, base);
	t->ns = this->ns;
	this->array_types[base] = t;
	return t;
}

Jet::Type* Compilation::GetInternalArrayType(Jet::Type* base, unsigned int size)
{
	auto res = this->internal_array_types.find(std::pair<Jet::Type*, unsigned int>(base, size));
	if (res != this->internal_array_types.end())
		return res->second;

	Type* t = new Type(this, base->name + "[" + std::to_string(size) + "]", Types::InternalArray, base);
	t->size = size;
	t->ns = this->ns;
	this->internal_array_types[std::pair<Jet::Type*, unsigned int>(base, size)] = t;
	return t;
}


::Jet::Type* Compilation::GetFunctionType(::Jet::Type* return_type, const std::vector<::Jet::Type*>& args)
{
	int key = (uintptr_t)return_type;
	for (auto arg : args)
		key ^= (uintptr_t)arg;

	bool found = false;
	Type* res = 0;
	auto f = this->function_types.equal_range(key);

	//search to see
	for (auto it = f.first; it != f.second; it++)
	{
		if (it->second->function->return_type == return_type && it->second->function->args.size() == args.size())
		{
			found = true;
			for (unsigned int i = 0; i < it->second->function->args.size(); i++)
			{
				if (it->second->function->args[i] != args[i])
					found = false;
			}
			//match args now
			if (found)
			{
				res = it->second;
				break;
			}
		}
	}

	// if we didnt find it cached, make it
	if (found == false)
	{
		auto t = new FunctionType;
		t->args = args;
		t->return_type = return_type;

		for (auto arg : args)
		{
			if (arg->type == Types::Void)
				this->Error("Void is not a valid function argument type", *this->current_function->current_token);
		}

		auto type = new Type(this, "", Types::Function);
		type->function = t;
		type->ns = global;
		type->name = type->ToString();

		global->members.insert({ type->name, type });// all raw function types go in global namespace

		function_types.insert({ key, type });
		return type;
	}
	return res;
}

void Compilation::ResolveTypes()
{
	auto oldns = this->ns;
	for (unsigned int i = 0; i < this->unresolved_types.size(); i++)
	{
		auto loc = unresolved_types[i].second;
		if ((*loc)->type == Types::Invalid)
		{
			//resolve me
			this->current_function->current_token = (*loc)->_location;
			this->ns = unresolved_types[i].first;

			if ((*loc)->pointer_type == (Type*)-7)
				this->typecheck = true;
			auto res = this->LookupType((*loc)->name, false);

			this->typecheck = false;
			delete *loc;//free the temporary

			*loc = res;
		}
	}
	this->unresolved_types.clear();
	this->ns = oldns;
}

Function* Compilation::GetFunction(const std::string& name, const std::vector<Type*>& args)
{
	auto next = this->ns;
    bool namespaced = name.find_first_of(':') != std::string::npos;
    if (namespaced)
    {
        // First find the top level namespace indicated by this
        Namespace* new_ns = 0;
		Namespace* cur_ns = this->ns;

        int cur_pos = 0;

        bool first = true;
        do
        {
            int len = 0;
	        while (cur_pos+len < name.length() &&
                   (IsLetter(name[cur_pos+len]) || IsNumber(name[cur_pos + len])))
	        {
		        len++;
	        }
            if (cur_pos+len >= name.length()-1)
            {
                break;// stop before we hit the last bit
            }

            std::string ns = name.substr(cur_pos, len);

		    // Try and find this first namespace by recursively going up
            // If this is not the top level, only check the current namespace
            new_ns = 0;
		    do
		    {
			    auto res = cur_ns->members.find(ns);
			    if (res != cur_ns->members.end())
			    {
			    	new_ns = res->second.ns;
			    	break;
			    }
			    cur_ns = cur_ns->parent;
		    }
		    while (cur_ns && first);

            first = false;
            if (!new_ns)
            {
                Error("Could not find namespace '" + ns + "'", *current_function->current_token);
            }
            cur_ns = new_ns;
            cur_pos += len + 2;
        }
        while (true);

        auto r = new_ns->members.equal_range(name);
	    for (auto it = r.first; it != r.second; it++)
		{
		    if (it->second.type == SymbolType::Function)
		    {
		    	if (it->second.fn->arguments_.size() == args.size())
		    	{
		    		return it->second.fn;
		    	}
		    }
	    }
    }
    else
    {
	    // Search down the namespace tree for the function if its not namespaced
	    do
	    {
		    auto r = next->members.equal_range(name);
	    	for (auto it = r.first; it != r.second; it++)
		    {
		    	if (it->second.type == SymbolType::Function)
		    	{
		    		if (it->second.fn->arguments_.size() == args.size())
		    		{
		    			return it->second.fn;
		    		}
		    	}
	    	}

    		next = next->parent;
    	}
        while (next && !namespaced);
    }
	return 0;
}

Jet::Symbol Compilation::GetVariableOrFunction(const std::string& name)
{
    bool namespaced = name.find_first_of(':') != std::string::npos;
    if (namespaced)
    {
        // First find the top level namespace indicated by this
        Namespace* new_ns = 0;
		Namespace* cur_ns = this->ns;

        int cur_pos = 0;

        bool first = true;
        do
        {
            int len = 0;
	        while (cur_pos+len < name.length() &&
                   (IsLetter(name[cur_pos+len]) || IsNumber(name[cur_pos + len])))
	        {
		        len++;
	        }
            if (cur_pos+len >= name.length()-1)
            {
                break;// stop before we hit the last bit
            }

            std::string ns = name.substr(cur_pos, len);

		    // Try and find this first namespace by recursively going up
            // If this is not the top level, only check the current namespace
            new_ns = 0;
		    do
		    {
			    auto res = cur_ns->members.find(ns);
			    if (res != cur_ns->members.end())
			    {
			    	new_ns = res->second.ns;
			    	break;
			    }
			    cur_ns = cur_ns->parent;
		    }
		    while (cur_ns && first);

            first = false;
            if (!new_ns)
            {
                Error("Could not find namespace '" + ns + "'", *current_function->current_token);
            }
            cur_ns = new_ns;
            cur_pos += len + 2;
        }
        while (true);

        auto r = new_ns->members.find(name.substr(cur_pos));
		if (r != new_ns->members.end() && (r->second.type == SymbolType::Function || r->second.type == SymbolType::Variable))
        {
			return r->second;
        }
    }
    else
    {
	    //search up the namespace tree for this variable or function
	    auto next = this->ns;
	    do
	    {
		    auto r = next->members.find(name);
	    	if (r != next->members.end() && (r->second.type == SymbolType::Function || r->second.type == SymbolType::Variable))
            {
		    	return r->second;
            }

	    	next = next->parent;
    	}
        while (next && !namespaced);
    }	
    return Symbol();
}

// For the racer, pretty broken atm
Jet::Function* Compilation::GetFunctionAtPoint(const char* file, int line)
{
	for (auto ii : this->functions)
	{
		if (ii->expression_)
		{
			auto block = ii->expression_->GetBlock();
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

