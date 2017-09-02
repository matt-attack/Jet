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
#include <sstream>


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

extern std::string executable_path;
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
	const std::vector<std::string>& resolved_deps = project->ResolveDependencies();
	for (int i = 0; i < deps; i++)
	{
		auto ii = project->dependencies[i];

		if (resolved_deps[i].length() == 0)
		{
			printf("Project: %s could not be found!", ii.c_str());
			delete project;
			return 0;
		}

		ii = resolved_deps[i];

		if (ii[0] != '.' && ii.find('/') == -1 && ii.find('\\') == -1)
		{
			//ok, search for the p/ackage using our database, we didnt give a path to one
			std::string path = this->FindProject(ii, "0.0.0");
		}

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
		} while (true);
	}

	std::vector<time_t> modifiedtimes;
	struct stat data;
	int x = stat("project.jp", &data);

	modifiedtimes.push_back(data.st_mtime);

	for (auto ii : project->files)
	{
		struct stat data;
		int x = stat(ii.c_str(), &data);

		modifiedtimes.push_back(data.st_mtime);
	}

	//lets look at the .jlib modification time
	//this need to use the correct paths from the located dependencies
	for (auto ii : resolved_deps)//project->dependencies)
	{
		std::string path = ii;
		path += "/build/symbols.jlib";
		struct stat data;
		int x = stat(path.c_str(), &data);

		modifiedtimes.push_back(data.st_mtime);
	}

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

		//ok now lets add it to the project cache, lets be sure to always save a backup though and need to get current path of the jetc 
		if (executable_path.length())
		{
			this->UpdateProjectList(project);
		}

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


std::vector<Compiler::ProjectInfo> Compiler::GetProjectList()
{
	//ok, now we need to remove our executable name from this
	int pos = executable_path.find_last_of('\\');
	if (pos == -1)
		pos = executable_path.find_last_of('/');
	std::string path = executable_path.substr(0, pos);
	std::string db_filename = path + "/project_database.txt";

	std::ifstream file(db_filename, std::ios_base::binary);
	bool found = false;
	std::string line;

	std::vector<Compiler::ProjectInfo> vec;
	while (std::getline(file, line))
	{
		std::istringstream iss(line);
		std::string name, path, version;

		//there is three parts, name, path and version separated by | and delimited by lines
		int first = line.find_first_of('|');
		int last = line.find_last_of('|');
		if (first == -1 || first == last)
			continue;//invalid line

		name = line.substr(0, first);
		path = line.substr(first + 1, last - 1 - first);
		version = line.substr(last + 1, line.length() - last);
		
		vec.push_back({ name, path, version });
	}
	return vec;
}

std::string Compiler::FindProject(const std::string& project_name, const std::string& desired_version)
{
	//ok, now we need to remove our executable name from this
	printf("Executable Path: %s\n", executable_path.c_str());
	int pos = executable_path.find_last_of('\\');
	if (pos == -1)
		pos = executable_path.find_last_of('/');
	std::string path = executable_path.substr(0, pos);
	std::string db_filename = path + "/project_database.txt";

	std::ifstream file(db_filename, std::ios_base::binary);
	bool found = false;
	std::string line;
	while (std::getline(file, line))
	{
		std::istringstream iss(line);
		std::string name, path, version;

		//there is three parts, name, path and version separated by | and delimited by lines
		int first = line.find_first_of('|');
		int last = line.find_last_of('|');
		if (first == -1 || first == last)
			continue;//invalid line

		name = line.substr(0, first);
		path = line.substr(first + 1, last - 1 - first);
		version = line.substr(last + 1, line.length() - last);

		//todo if no version is specified, we want the newest version
		if (name == project_name)
		{
			//if (path != curpath)
			//{
			//	printf("TWO PACKAGES WITH THE SAME NAME EXIST, THIS CAN CAUSE ISSUES!\n");
			//}

			/*if (version != desired_version)
			{
				//if the path is right, but the version changed we need to edit the version
				printf("PACKAGE VERSION MISMATCH NEED TO HANDLE THIS\n");
			}*/

			return path;

			//we found it, lets return the path
			found = true;
			break;
		}
	}
	return "";
}

void Compiler::UpdateProjectList(JetProject* project)
{
	//ok, now we need to remove our executable name from this
	int pos = executable_path.find_last_of('\\');
	if (pos == -1)
		pos = executable_path.find_last_of('/');
	std::string path = executable_path.substr(0, pos);
	std::string db_filename = path + "/project_database.txt";

	char curpath[500];
	getcwd(curpath, 500);//todo escape our filename and project name and make parser above able to read it out

	//ok, lets scan through the database to see if we are in it
	//todo: break this search out into a function
	std::ifstream file(db_filename, std::ios_base::binary);
	bool found = false;
	std::string line;
	while (std::getline(file, line))
	{
		std::istringstream iss(line);
		std::string name, path, version;

		//there is three parts, name, path and version separated by | and delimited by lines
		int first = line.find_first_of('|');
		int last = line.find_last_of('|');
		if (first == -1 || first == last)
			continue;//invalid line

		name = line.substr(0, first);
		path = line.substr(first + 1, last - 1 - first);
		version = line.substr(last + 1, line.length() - last);

		if (name == project->project_name)
		{
			if (path != curpath)
			{
				printf("TWO PACKAGES WITH THE SAME NAME EXIST, THIS CAN CAUSE ISSUES!\n");
			}

			/*if (version != project->version)
			{
				//if the path is right, but the version changed we need to edit the version
				printf("PACKAGE VERSION MISMATCH NEED TO HANDLE THIS\n");
			}*/
			found = true;
			break;
		}
	}

	//if we arent in it, backup the old one, then append ourselves to the new one
	if (found == false)
	{
		//perform copy
		{
			std::ifstream source(db_filename, std::ios::binary);
			std::ofstream dest(db_filename + ".backup", std::ios::binary);

			std::istreambuf_iterator<char> begin_source(source);
			std::istreambuf_iterator<char> end_source;
			std::ostreambuf_iterator<char> begin_dest(dest);
			copy(begin_source, end_source, begin_dest);

			source.close();
			dest.close();
		}

		//append our name
		std::ofstream file(db_filename, std::ios_base::app);
		file << project->project_name << "|" << curpath << '|' << project->version << '\n';
	}
}