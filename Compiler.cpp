#include "Compiler.h"
#include "CompilerContext.h"

#include "Source.h"
#include "Parser.h"
#include "Lexer.h"
#include "Expressions.h"
#include "UniquePtr.h"
#include "Project.h"
#include "OptionParser.h"

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#include <fstream>


using namespace Jet;

//#include "JIT.h"
//MCJITHelper* JITHelper;
#ifdef _WIN32
#include <Windows.h>

#include <sys/stat.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

//this adds all the available options to the parser for jet
void Jet::SetupDefaultCommandOptions(OptionParser* parser)
{
	parser->AddOption("o", "0");
	parser->AddOption("f", "0");
	parser->AddOption("t", "0");
	parser->AddOption("r", "0");
	parser->AddOption("target", "");
	parser->AddOption("linker", "");
	parser->AddOption("debug", "2");
}

//this reads the options out of the parser and applys them 
void CompilerOptions::ApplyOptions(OptionParser* parser)
{
	this->optimization = parser->GetOption("o").GetInt();
	this->force = parser->GetOption("f").GetString().length() == 0;
	this->time = parser->GetOption("t").GetString().length() == 0;
	this->run = parser->GetOption("r").GetString().length() == 0;
	this->target = parser->GetOption("target").GetString();
	this->linker = parser->GetOption("linker").GetString();
	this->debug = parser->GetOption("debug").GetInt();

	//todo error if we have other options specified
}

void Jet::Diagnostic::Print()
{
	if (token.type != TokenType::InvalidToken)
	{
		int startrow = token.column;
		int endrow = token.column + token.text.length();

		std::string code = this->line;
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
		printf("[error] %s %d:%d to %d:%d: %s\n[error] >>>%s\n[error] >>>%s\n\n", this->file.c_str(), token.line, startrow, token.line, endrow, message.c_str(), code.c_str(), underline.c_str());
	}
	else
	{
		//just print something out, it was probably a build system error, not one that occurred in the code
		printf("[error] %s: %s\n", this->file.c_str(), message.c_str());
	}
}

class MemberRenamer : public ExpressionVisitor
{
	std::string stru, member, newname;
	Compilation* compiler;
public:

	MemberRenamer(std::string stru, std::string member, std::string newname, Compilation* compiler) : stru(stru), member(member), newname(newname), compiler(compiler)
	{

	}

	virtual void Visit(CallExpression* expr)
	{

	}

	virtual void Visit(StructExpression* expr)
	{
		if (expr->GetName() == stru)
		{
			for (auto ii : expr->members)
			{
				if (ii.type == StructMember::VariableMember)
				{
					ii.variable.name.text = newname;
				}
			}
		}
	}

	virtual void Visit(IndexExpression* expr)
	{
		if (expr->member.text.length() > 0 && expr->member.text == member)
		{
			auto type = expr->GetBaseType(compiler);
			if ((expr->token.type == TokenType::Dot && type->type == Types::Struct && type->data->name == stru) || (expr->token.type == TokenType::Pointy && type->base->type == Types::Struct))
			{
				expr->member.text = newname;
			}
		}
	}
};

#include <process.h>
#include <thread>
void ExecuteProject(JetProject* project, const char* projectdir)
{
	//now try running it if we are supposed to
#ifdef _WIN32
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);

	ZeroMemory(&pi, sizeof(pi));

	std::string path = "build\\" + project->project_name + ".exe ";
	CreateProcess(path.c_str(), "", 0, 0, 0, CREATE_NEW_CONSOLE, 0, 0, &si, &pi);

	//throw up a thread that closes the handle when its done
	std::thread x([pi](){
		// Wait until child process exits.
		WaitForSingleObject(pi.hProcess, INFINITE);

		// Close process and thread handles. 
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	});
	x.detach();
#endif
	//system(path.c_str());
	
	//spawnl(P_NOWAIT, "cmd.exe", "cmd.exe", path.c_str(), 0);
}

int Compiler::Compile(const char* projectdir, CompilerOptions* optons, const std::string& confg_name, OptionParser* parser)
{
	JetProject* project = JetProject::Load(projectdir);
	if (project == 0)
		return 0;

	char olddir[1000];
	getcwd(olddir, 1000);
	std::string path = projectdir;
	path += '/';

	if (path.length() > 1)
		chdir(path.c_str());

	//build each dependency
	bool needs_rebuild = false;
	int deps = project->dependencies.size();
	for (int i = 0; i < deps; i++)
	{
		auto ii = project->dependencies[i];
		//spin up new compiler instance and build it
		Compiler compiler;
		auto success = compiler.Compile(ii.c_str());
		if (success == 0)
		{
			printf("Dependency compilation failed, stopping compilation");

			//restore working directory
			chdir(olddir);
			return 0;
		}
		else if (success == 2)//compilation was successful but a rebuild was done, so we also need to rebuild
		{
			needs_rebuild = true;
		}
	}

	printf("\nCompiling Project: %s\n", projectdir);

	//read in buildtimes
#if 0 //def false//_WIN32
	std::vector<std::pair<int, int>> buildtimes;
#else
	std::vector<int> buildtimes;
#endif
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
#if 0 //def false//_WIN32
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
#else
			int hi;
			sscanf(line.c_str(), "%i", &hi);

			if (first)
			{
				compiler_version = line;
				first = false;
			}
			else
			{
				buildtimes.push_back(hi);
			}
#endif
		} while (true);
	}


#if 0 //def _WIN32
	std::vector<std::pair<int, int>> modifiedtimes;
	auto file = CreateFileA("project.jp", GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, 0, NULL);
	FILETIME create, modified, access;
	GetFileTime(file, &create, &access, &modified);
	CloseHandle(file);
	modifiedtimes.push_back({ modified.dwHighDateTime, modified.dwLowDateTime });
#else
	std::vector<time_t> modifiedtimes;
	struct stat data;
	int x = stat("project.jp", &data);

	modifiedtimes.push_back(data.st_mtime);
#endif

	for (auto ii : project->files)
	{
#if 0 //_WIN32
		auto file = CreateFileA(ii.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
			OPEN_EXISTING, 0, NULL);
		FILETIME create, modified, access;
		GetFileTime(file, &create, &access, &modified);

		CloseHandle(file);
		modifiedtimes.push_back({ modified.dwHighDateTime, modified.dwLowDateTime });
#else
		struct stat data;
		int x = stat(ii.c_str(), &data);

		modifiedtimes.push_back(data.st_mtime);
#endif
	}
	//todo: push back times for dependencies

	std::string config_name = "";
	if (parser)
	{
		SetupDefaultCommandOptions(parser);

		if (parser->commands.size() > 1)
			config_name = parser->commands[1];
	}

	//get the config, or default
	JetProject::BuildConfig configuration;
	if (config_name.length() > 0)
	{
		//find the configuration
		bool found = false;
		for (auto ii : project->configurations)
		{
			if (ii.name == confg_name)
			{
				configuration = ii;
				found = true;
			}
		}
		if (found == false && project->configurations.size() > 0)
		{
			configuration = *project->configurations.begin();
			printf("Build Configuration Name: '%s' Does Not Exist, Defaulting To '%s'\n", config_name.c_str(), project->configurations.begin()->name.c_str());
		}
		else if (found == false)
			printf("Build Configuration Name: '%s' Does Not Exist\n", config_name.c_str());
	}
	else
	{
		if (project->configurations.size() > 0)
			configuration = *project->configurations.begin();
	}

	//read in config from command and stuff
	CompilerOptions options;
	if (parser)
	{
		parser->Parse(configuration.options);

		options.ApplyOptions(parser);
	}
	else
	{
		OptionParser p;
		SetupDefaultCommandOptions(&p);
		p.Parse(configuration.options);

		options.ApplyOptions(&p);
	}

	//add options to this later
	FILE* jlib = fopen("build/symbols.jlib", "rb");
	FILE* output = fopen(("build/" + project->project_name + ".o").c_str(), "rb");
	if (options.force || needs_rebuild)
	{
		//the rebuild was forced
	}
	else if (strcmp(__TIME__, compiler_version.c_str()) != 0)//see if the compiler was the same
	{
		//do a rebuild compiler version is different
	}
	else if ((project->IsExecutable() == false && jlib == 0) || output == 0)//check if .jlib or .o exists
	{
		//output file missing, do a rebuild
	}
	else if (modifiedtimes.size() == buildtimes.size())//check if files were modified
	{
		for (int i = 0; i < modifiedtimes.size(); i++)
		{
			if (modifiedtimes[i] == buildtimes[i])
			{
				if (i == modifiedtimes.size() - 1)
				{
					printf("No Changes Detected, Compiling Skipped\n");

					if (options.run)
						ExecuteProject(project, projectdir);

					//restore working directory
					chdir(olddir);
					return 1;
				}
			}
			else
			{
				break;
			}
		}
	}
	if (jlib) fclose(jlib);
	if (output) fclose(output);

	//running prebuild
	if (configuration.prebuild.length() > 0)
		printf("%s", exec(configuration.prebuild.c_str()).c_str());

	DiagnosticBuilder diagnostics([](Diagnostic& msg)
	{
		msg.Print();
	});
	Compilation* compilation = Compilation::Make(project, &diagnostics, options.time, options.debug);

error:

	if (compilation == 0)
	{
		//compiling failed completely

		//restore working directory
		chdir(olddir);
		return 0;
	}
	else if (compilation->GetErrors().size() > 0)
	{
		printf("Compiling Failed: %d Errors Found\n", compilation->GetErrors().size());

		delete compilation;

		//restore working directory
		chdir(olddir);
		return 0;
	}
	else
	{
		compilation->Assemble(options.target, options.linker, options.optimization, options.time);

		//output build times
		std::ofstream rebuild("build/rebuild.txt");
		//output compiler version
		rebuild.write(__TIME__, strlen(__TIME__));
		rebuild.write("\n", 1);
		for (auto ii : modifiedtimes)
		{
			char str[150];
#if 0 //_WIN32
			int len = sprintf(str, "%i,%i\n", ii.first, ii.second);
#else
			int len = sprintf(str, "%i\n", ii);
#endif
			rebuild.write(str, len);
		}
		//todo: output for dependencies too

		printf("Project built successfully.\n\n");

		//running postbuild
		if (configuration.postbuild.length() > 0)
			printf("%s", exec(configuration.postbuild.c_str()).c_str());

		if (options.run && project->IsExecutable())
		{
			ExecuteProject(project, projectdir);
			Sleep(50);//give it a moment to run, for some reason derps up without this
		}
		//todo: need to look at compilation time of dependencies to see if we need to rebuild
		delete compilation;

		//restore working directory
		chdir(olddir);
		return 2;
	}

	delete compilation;

	//restore working directory
	chdir(olddir);
	return 1;
}