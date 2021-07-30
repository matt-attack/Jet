
#define _CRTDBG_MAP_ALLOC

#include <stdio.h>
#include <stdlib.h>

#include <stack>
#include <vector>

#define CODE(code) #code

#include "../Project.h"
#include "../Compiler.h"
#include "../Expressions.h"

#include <iostream>
#include <string>
#include <fstream>
#include <streambuf>
#include <math.h>
#include <functional>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

using namespace Jet;


#include <sstream>
//#include <clang-c/Index.h>

#include "../arg_parse.h"
//#include "OptionParser.h"

#ifdef _DEBUG
#ifndef DBG_NEW
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#define new DBG_NEW
#endif
#endif  // _DEBUG

void MakeDocs(Compilation* compilation)
{
	//lets output some docs
	//this needs to be recursive
	std::string out;
	out += //"<!DOCTYPE html>\n"
		"<html>\n"
		"<head>\n"
		"<style>\n"
		"table, th, td{\n"
		"border: 1px solid black;\n"
		"}\n"
		"</style>\n"
		"</head>\n"
		"<body>\n";
	out += "<table style=\"width:80%\">\n";
	out += "<tr>\n";
	out += "\t<th>Name</th>\n";
	out += "\t<th>Description</th>\n";
	out += "</tr>\n";

	for (auto mem : compilation->ns->members)
	{
		if (mem.second.type == Jet::SymbolType::Namespace)
		{
			//output a new namespace block and recurse
		}
		else if (mem.second.type == Jet::SymbolType::Function)
		{
			out += "<tr>\n";
			out += "\t<td>" + mem.first + "</td>\n";
			out += "\t<td>Returns " + mem.second.fn->return_type->name + "</td>\n";
			out += "</tr>\n";
		}
		else if (mem.second.type == Jet::SymbolType::Type)
		{
			if (mem.second.ty->type == Types::Pointer)
				continue;
			out += "<tr>\n";
			out += "\t<td>" + mem.first + "</td>\n";
			out += "\t<td>" + mem.second.ty->name + "</td>\n";
			out += "</tr>\n";

			//mem.second.ty->data->expression
		}
	}
	out += "</table>";
	out += "</body></html>";
	/*Target of the Link :

	<a name = "name_of_target">Content< / a>

	Link to the Target :

	<a href = "#name_of_target">Link Text< / a>*/
	//todo use this and add description
	auto of = std::ofstream("build/docs.html", std::ios_base::out);
	of << out;
}

int DoCommand(int argc, const char* argv[])
{
	std::string cmd = argc > 1 ? argv[1] : "";
	if (cmd == "projects")
	{
		//print out all the projects and versions
		auto projects = Compiler::GetProjectList();

		for (auto proj : projects)
		{
			if (proj.version.find('\r') != -1)
				proj.version.pop_back();
			printf("'%s' version '%s' at %s\n", proj.name.c_str(), proj.version.c_str(), proj.path.c_str());
			//std::cout << proj.name << " " << proj.version << " " << proj.path << "\n";
		}
		return 0;
	}
    else if (cmd == "compile")
    {
        ArgParser parser;
        parser.SetUsage("jet compile FILES...\n\nCompile a set of jet source files.");
        auto output = parser.AddMulti({"o", "output"}, "Output file name.", "a.out");
        auto libs = parser.AddMulti({"l", "lib"}, "Add libraries to link.", "", true);
        auto defines = parser.AddMulti({"D", "define"}, "Add a compile time definition.", "", true);
        parser.Parse(argv, argc, 1);

        auto files = parser.GetAllPositional();

        // generate a dummy project with all of these files
        JetProject* p = JetProject::Create();
        p->project_name = output->GetString();
        p->files = files;
        p->version = "0.0.1";
        p->libs = libs->values;// todo
        p->defines.clear();// todo
        p->is_executable = true;// todo add an argument for this

        for (auto x: defines->values)
        {
            p->defines[x] = true;
        }

        DiagnosticBuilder b([](Diagnostic& x) {x.Print(); });
        auto compilation = Compilation::Make(p, &b);

        std::vector<std::string> resolved_deps;
	    compilation->Assemble(resolved_deps);

        delete compilation;
        delete p;
        return 0;
    }
	else if (cmd == "convert")
	{
		std::string two = argv[2];
		//fix conversion of attributes for calling convention and fix function pointers
		std::string str;// = generate_jet_from_header(two.c_str());
		if (str.length() == 0)
        {
			printf("No such file.");
        }
		else
		{
			std::ofstream o(two + ".jet");
			o << str;
			o.close();
		}
		return 0;
	}
	else if (cmd == "new")
	{
		std::string name = argv[2];

		mkdir(name.c_str(), 0x755);

		//insert a project file and a main code file

		//project file
		std::string project_file_name = name + "/project.jp";
		FILE* f = fopen(project_file_name.c_str(), "wb");
		const char* project = "requires: jetc\n"
			"files: main.jet\n"
			"libs:\n"
			"defines:\n"

			"[debug]\n"
			"prebuild: \"\"\n"
			"postbuild: \"\"\n"
			"config: \"\"\n\n"

			"[release]\n"
			"prebuild: \"\"\n"
			"postbuild: \"\"\n"
			"config: \"-d1 -o3\"\n";
		fwrite(project, strlen(project), 1, f);
		fclose(f);

		//write the main.jet file
		std::string main_file_name = name + "/main.jet";
		f = fopen(main_file_name.c_str(), "wb");
		const char* main = "//This is your main function\n"
			"fun int main()\n"
			"{\n"
			"\treturn 1;\n"
			"}\n";
		fwrite(main, strlen(main), 1, f);
		fclose(f);

		printf("Created new empty project.\n");
		return 0;
	}
	else if (cmd == "build")
	{
        ArgParser parser;
        parser.SetUsage("jet build PROJECT <configuration>\n\nCompile a jet project and any dependencies.");
        auto time = parser.AddMulti({"t", "time"}, "Time various stages of compilation.", "");
        auto run = parser.AddMulti({"r", "run"}, "Run the program after compilation completes.", "");
        auto force = parser.AddMulti({"f", "force"}, "Force recompilation.", "");
        auto optimization = parser.AddMulti({"O", "optimization"}, "Optimization level.", "0");
        auto debug = parser.AddMulti({"debug"}, "Debug output level.", "2");
        auto linker = parser.Add("linker", "Set linker type.", "");
        auto target = parser.Add("target", "Set target string.", "");
        auto output_ir = parser.AddMulti({"ir"}, "Output LLVM IR to the build directory.", "");
        parser.Parse(argv, argc, 1);

		CompilerOptions options;
        options.force = force->GetBool();
        options.time = time->GetBool();
        options.run = run->GetBool();
        options.optimization = optimization->GetInt();
        options.linker = linker->GetString();
        options.debug = debug->GetInt();
        options.target = target->GetString();
        options.output_ir = output_ir->GetBool();

		std::string config = parser.GetPositional(1);

        std::string projectdir = parser.GetPositional(0);
        std::unique_ptr<JetProject> project(JetProject::Load(projectdir.c_str()));
	    if (project == 0)
		    return -1;

		Jet::Compiler c;
		return c.Compile(project.get(), &options, config) == 0 ? -1 : 0;
	}
    else if (cmd == "help" || cmd == "--h" || cmd == "--help" || cmd == "-h")
    {
        
    }
	else
	{
		printf("Unknown verb.\n");
	}
    printf("Usage: jet VERB <args...>\n\nVerbs:\n  build\n  compile\n  projects\n");
    return -1;
}

#if (BOOST_OS_CYGWIN || _WINDOWS)

#include <Windows.h>

std::string exec_path(const char *argv0)
{
	char buf[1024] = { 0 };
	DWORD ret = GetModuleFileNameA(NULL, buf, sizeof(buf));
	if (ret == 0 || ret == sizeof(buf))
	{
		//return executable_path_fallback(argv0);
	}
	return buf;
}
#else

#include <unistd.h>
std::string exec_path(const char* argv0)
{
  char dest[PATH_MAX];
  memset(dest,0,sizeof(dest)); // readlink does not null terminate!
  if (readlink("/proc/self/exe", dest, PATH_MAX) == -1) {
    perror("readlink");
  } else {
    //printf("%s\n", dest);
  }
  return dest;
}
#endif

#include <llvm/Support/ErrorHandling.h>
#include <llvm-c/ErrorHandling.h>

std::string executable_path;
int main(int argc, const char* argv[])
{
#ifdef _WIN32
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	//fix llvm being super dumb and calling exit...
	LLVMInstallFatalErrorHandler([](const char* reason)
	{
		throw 7;
	});

	executable_path = exec_path(argv[0]);

	//look for config file, and create default if not there

	if (argc > 1)//if we get a command, just try and build the project at that path
	{
		return DoCommand(argc, argv);
	}

	//auto path = getenv("PATH");
	//std::string newpath = path;
	//newpath += ';';
	//newpath += 
	//putenv("PATH=")
	//need way to configure jet to have a libraries folder that we can add to path

	printf("Input the path to a project folder to build it\n");
	while (true)
	{
		printf("\n>");
		char command[800];
		std::cin.getline(command, 800);

		//split the string
		{
			const char* args[400] = {};
			int numargs = 0;
			int i = 0; bool inquotes = false;

			//add a dummy arg with the current location
			numargs = 1;
			args[0] = "~ignore me~";

			while (command[i])
			{
				if (command[i] == '"')
				{
					inquotes = !inquotes;
					if (inquotes == false)
						command[i] = 0;
				}
				else if (command[i] == ' ' && inquotes == false)
				{
					command[i] = 0;

					numargs++;
				}
				else
				{
					if (args[numargs] == 0)
						args[numargs] = &command[i];
				}
				i++;
			}
			if (args[numargs] != 0)
				numargs++;

			DoCommand(numargs, args);
#ifdef _WIN32
			//_CrtDumpMemoryLeaks();
#endif
			continue;
		}
	}

	return 0;
}

