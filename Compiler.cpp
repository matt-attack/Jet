#include "Compiler.h"
#include "CompilerContext.h"

#include "Source.h"
#include "Parser.h"
#include "Lexer.h"
#include "Expressions.h"
#include "UniquePtr.h"
#include "Project.h"

#include <direct.h>

#include <fstream>
#include <filesystem>


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


extern Source* current_source;

void Jet::Diagnostic::Print()
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
					ii.variable.second.text = newname;
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

#include "OptionParser.h"

bool Compiler::Compile(const char* projectdir, CompilerOptions* optons, const std::string& confg_name, OptionParser* parser)
{
	JetProject* project = JetProject::Load(projectdir);
	if (project == 0)
		return 0;

	char olddir[500];
	getcwd(olddir, 500);
	std::string path = projectdir;
	path += '/';

	chdir(path.c_str());

	//build each dependency
	int deps = project->dependencies.size();
	for (int i = 0; i < deps; i++)
	{
		auto ii = project->dependencies[i];
		//spin up new compiler instance and build it
		Compiler compiler;
		auto success = compiler.Compile(ii.c_str());
		if (success == false)
		{
			printf("Dependency compilation failed, stopping compilation");

			//restore working directory
			chdir(olddir);
			return false;
		}
	}

	printf("\nCompiling Project: %s\n", projectdir);

	//read in buildtimes
#ifdef false//_WIN32
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
#ifdef false//_WIN32
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


#ifdef false _WIN32
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
#ifdef false//_WIN32
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

	//OptionParser parser;
	std::string config_name = "";
	if (parser)
	{
		parser->AddOption("o", "0");
		parser->AddOption("f", "0");

		if (parser->commands.size())
			config_name = parser->commands.front();
	}

	//get the config, or default
	JetProject::BuildConfig configuration;
	if (config_name.length() > 0)
	{
		auto f = project->configurations.find(config_name);
		if (f != project->configurations.end())
			configuration = f->second;
	}
	else
	{
		if (project->configurations.size() > 0)
			configuration = project->configurations.begin()->second;
	}

	//read in config from command and stuff
	CompilerOptions options;
	if (parser)
	{
		parser->Parse(configuration.options);
		options.optimization = parser->GetOption("o").GetInt();
		options.force = parser->GetOption("f").GetString().length() == 0;
	}
	else
	{
		OptionParser p;
		p.AddOption("o", "0");
		p.AddOption("f", "0");
		p.Parse(configuration.options);

		options.optimization = p.GetOption("o").GetInt();
		options.force = p.GetOption("f").GetString().length() == 0;
	}

	//add options to this later
	FILE* jlib = fopen("build/symbols.jlib", "rb");
	FILE* output = fopen(("build/" + project->project_name + ".o").c_str(), "rb");
	if (options.force)// && options->force)
	{
		//the rebuild was forced
	}
	else if (strcmp(__TIME__, compiler_version.c_str()) != 0)//see if the compiler was the same
	{
		//do a rebuild compiler version is different
	}
	else if (jlib == 0 || output == 0)//check if .jlib or .o exists
	{
		//output file missing, do a rebuild
	}
	else if (modifiedtimes.size() == buildtimes.size())//check if files were modified
	{
		for (int i = 0; i < modifiedtimes.size(); i++)
		{
#ifdef false//_WIN32
			if (modifiedtimes[i].first == buildtimes[i].first && modifiedtimes[i].second == buildtimes[i].second)
#else
			if (modifiedtimes[i] == buildtimes[i])// && modifiedtimes[i].second == buildtimes[i].second)
#endif
			{
				if (i == modifiedtimes.size() - 1)
				{
					printf("No Changes Detected, Compiling Skipped\n");

					//restore working directory
					chdir(olddir);
					return true;
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
	Compilation* compilation = Compilation::Make(project, &diagnostics);

error:

	if (compilation == 0)
	{
		//compiling failed completely
	}
	else if (compilation->GetErrors().size() > 0)
	{
		//for (auto ii : compilation->GetErrors())
			//ii.Print();

		printf("Compiling Failed: %d Errors Found\n", compilation->GetErrors().size());
	}
	else
	{
		compilation->Assemble(options.optimization);

		//output build times
		std::ofstream rebuild("build/rebuild.txt");
		//output compiler version
		rebuild.write(__TIME__, strlen(__TIME__));
		rebuild.write("\n", 1);
		for (auto ii : modifiedtimes)
		{
			char str[150];
#ifdef false //_WIN32
			int len = sprintf(str, "%i,%i\n", ii.first, ii.second);
#else
			int len = sprintf(str, "%i\n", ii);
#endif
			rebuild.write(str, len);
		}

		printf("Project built successfully.\n\n");

		//running postbuild
		if (configuration.postbuild.length() > 0)
			printf("%s", exec(configuration.postbuild.c_str()).c_str());
	}

	delete compilation;

	//restore working directory
	chdir(olddir);
	return true;
}