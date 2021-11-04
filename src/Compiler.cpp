#include "Compiler.h"
#include "CompilerContext.h"

#include "Source.h"
#include "Parser.h"
#include "Lexer.h"
#include "expressions/Expressions.h"
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

#ifdef _WIN32
#include <Windows.h>
#include <process.h>
#include <sys/stat.h>
#else
#include <sys/types.h>
#include <unistd.h>

void Sleep(int duration)
{
    usleep(duration*1000);
}
#endif

#include <thread>

//this adds all the available options to the parser for jet
/*void Jet::SetupDefaultCommandOptions(OptionParser* parser)
{
	parser->AddOption("o", "0", true);
	parser->AddOption("f", "0", true);
	parser->AddOption("t", "0", true);
	parser->AddOption("r", "0", true);
	parser->AddOption("target", "", false);
	parser->AddOption("linker", "", false);
	parser->AddOption("debug", "2", false);
	parser->AddOption("ir", "0", true);
}

//this reads the options out of the parser and applys them 
void CompilerOptions::ApplyOptions(OptionParser* parser)
{
	this->optimization = parser->GetOption("o").GetInt();
	this->force = parser->GetOption("f").GetBool();
	this->time = parser->GetOption("t").GetBool();
	this->run = parser->GetOption("r").GetBool();
	this->target = parser->GetOption("target").GetString();
	this->linker = parser->GetOption("linker").GetString();
	this->debug = parser->GetOption("debug").GetInt();
	this->output_ir = parser->GetOption("ir").GetBool();
}*/

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

void ExecuteProject(const JetProject* project)
{
	//now try running it if we are supposed to
#ifdef _WIN32
	STARTUPINFOA si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);

	ZeroMemory(&pi, sizeof(pi));

	std::string path = "build\\" + project->project_name + ".exe ";
	CreateProcessA(path.c_str(), "", 0, 0, 0, CREATE_NEW_CONSOLE, 0, 0, &si, &pi);

	//throw up a thread that closes the handle when its done
	std::thread x([pi](){
		// Wait until child process exits.
		WaitForSingleObject(pi.hProcess, INFINITE);

		// Close process and thread handles. 
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	});
	x.detach();
#else
	std::string cmd = "gnome-terminal -- build/" + project->project_name;
	system(cmd.c_str());
#endif
}

extern std::string executable_path;
int Compiler::Compile(const JetProject* project, const CompilerOptions* optons, const std::string& config_name)
{
	char olddir[1000];
	getcwd(olddir, 1000);
	std::string path = project->path;
	path += '/';

	if (path.length() > 1)
		chdir(path.c_str());

	//build each dependency
	bool needs_rebuild = false;
	int deps = project->dependencies.size();
	std::vector<std::string> resolved_deps;
    project->ResolveDependencies(resolved_deps);
	for (int i = 0; i < deps; i++)
	{
		auto ii = project->dependencies[i];

		if (resolved_deps[i].length() == 0)
		{
			printf("Dependency: %s could not be found!\nMake sure to build it at least once so we can find it.\n", ii.c_str());

			//make sure to restore old cwd
			chdir(olddir);

			return 0;
		}
		else
		{
			printf("Dependency \"%s\" resolved to %s.\n", ii.c_str(), resolved_deps[i].c_str());
		}

        if (optons && !optons->build_deps)
        {
            continue;
        }

		ii = resolved_deps[i];

		if (ii[0] != '.' && ii.find('/') == -1 && ii.find('\\') == -1)
		{
			//ok, search for the package using our database, we didnt give a path to one
			std::string path = this->FindProject(ii, "0.0.0");
		}

        // Try and load the package
        std::unique_ptr<JetProject> proj(JetProject::Load(ii));
        if (!proj)
        {
            printf("Could not load dependency project.\n");
            return 0;
        }

		//spin up new compiler instance and build it
		Compiler compiler;
		auto success = compiler.Compile(proj.get());
		if (success == 0)
		{
			printf("Dependency compilation failed, stopping compilation.\n");

			//restore working directory
			chdir(olddir);
			return 0;
		}
		else if (success == 2)//compilation was successful but a rebuild was done, so we also need to rebuild
		{
			needs_rebuild = true;
		}
	}

	printf("\nCompiling Project: %s\n", project->path.c_str());

	//read in buildtimes
	std::vector<int> buildtimes;
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
	for (auto ii : resolved_deps)
	{
		std::string path = ii;
		path += "/build/symbols.jlib";
		struct stat data;
		int x = stat(path.c_str(), &data);

		modifiedtimes.push_back(data.st_mtime);
	}

	//get the config, or default
	JetProject::BuildConfig configuration;
	if (config_name.length() > 0)
	{
		//find the configuration
		bool found = false;
		for (auto ii : project->configurations)
		{
			if (ii.name == config_name)
			{
				configuration = ii;
				found = true;
			}
		}
		if (found == false && project->configurations.size() > 0)
		{
			configuration = *project->configurations.begin();
			printf("WARNING: Build Configuration Name: '%s' Does Not Exist, Defaulting To '%s'\n", config_name.c_str(), project->configurations.begin()->name.c_str());
		}
		else if (found == false)
		{
			printf("WARNING: Build Configuration Name: '%s' Does Not Exist\n", config_name.c_str());
		}
	}
	else
	{
		if (project->configurations.size() > 0)
		{
			configuration = *project->configurations.begin();
		}
	}

	//read in config from command and stuff
	CompilerOptions options;
    if (optons)
    {
        options = *optons;
    }

    // todo revamp this
	/*{
		OptionParser p;
		SetupDefaultCommandOptions(&p);
		p.Parse(configuration.options);

		options.ApplyOptions(&p);
	}*/

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
		for (unsigned int i = 0; i < modifiedtimes.size(); i++)
		{
			if (modifiedtimes[i] == buildtimes[i])
			{
				if (i == modifiedtimes.size() - 1)
				{
					printf("No Changes Detected, Compiling Skipped\n");

					if (options.run && project->IsExecutable())
					{
						ExecuteProject(project);
						Sleep(50);//give it a moment to run, for some reason derps up without this
					}
					else if (options.run)
					{
						printf("Warning: Ignoring run option because program is not executable.\n");
					}

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

	//Run prebuild commands
	if (configuration.prebuild.length() > 0)
	{
		printf("%s", exec(configuration.prebuild.c_str()).c_str());
	}

	DiagnosticBuilder diagnostics([](Diagnostic& msg)
	{
		msg.Print();
	});

	std::unique_ptr<Compilation> compilation(Compilation::Make(project, &diagnostics, options.time, options.debug, options.target));

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
		printf("Compiling Failed: %zi Errors Found\n", compilation->GetErrors().size());

		//restore working directory
		chdir(olddir);
		return 0;
	}

	compilation->Assemble(resolved_deps, options.linker, options.optimization, options.time, options.output_ir);

	if (compilation->GetErrors().size() > 0)
	{
		goto error;
	}

	//output build times
	std::ofstream rebuild_out("build/rebuild.txt");
	//output compiler version
	rebuild_out.write(__TIME__, strlen(__TIME__));
	rebuild_out.write("\n", 1);
	for (auto ii : modifiedtimes)
	{
		char str[150];
#if 0 //_WIN32
		int len = sprintf(str, "%i,%i\n", ii.first, ii.second);
#else
		int len = sprintf(str, "%li\n", ii);
#endif
		rebuild_out.write(str, len);
	}

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
		ExecuteProject(project);
		Sleep(50);//give it a moment to run, for some reason derps up without this
	}
	else if (options.run)
	{
		printf("Warning: Ignoring run option because program is not executable.\n");
	}

	//restore working directory
	chdir(olddir);
	return 2;
}

std::string GetProjectDatabasePath()
{
	//ok, now we need to remove our executable name from this
	int pos = executable_path.find_last_of('\\');
	if (pos == -1)
		pos = executable_path.find_last_of('/');
	std::string path = executable_path.substr(0, pos);
	return path + "/project_database.txt";
}

std::vector<Compiler::ProjectInfo> Compiler::GetProjectList()
{
	std::ifstream file(GetProjectDatabasePath(), std::ios_base::binary);
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
	auto projects = GetProjectList();
	for (auto ii : projects)
	{
		//todo also check version
		if (ii.name == project_name)
		{
			return ii.path;
		}
	}

	return "";
}

void Compiler::UpdateProjectList(const JetProject* project)
{
    // dont add executable projects
    if (project->IsExecutable())
    {
        return;
    }

	char curpath[500];
	getcwd(curpath, 500);//todo escape our filename and project name and make parser above able to read it out

	std::string db_filename = GetProjectDatabasePath();
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
