
#include <stdio.h>
#include <stdlib.h>

#include <stack>
#include <vector>

#define CODE(code) #code

#include "Project.h"
#include "Compiler.h"
#include "Expressions.h"

#include <iostream>
#include <string>
#include <fstream>
#include <streambuf>
#include <math.h>
#include <functional>

using namespace Jet;


#include <sstream>

#include "OptionParser.h"

int main(int argc, char* argv[])
{
	if (argc > 1)//if we get a command, just try and build the project at that path
	{
		OptionParser parser;
		parser.AddOption("o", "0");
		parser.AddOption("f", "0");
		parser.Parse(argc, argv);

		Jet::Compiler c;
		if (strcmp(argv[1], "build") == 0)
			c.Compile("");
		else
			c.Compile(argv[1]);

		return 0;
	}

	printf("Input the path to a project folder to build it\n");
	while (true)
	{
		printf("\n>");
		char command[800];
		std::cin.getline(command, 800);

		Jet::Compiler c2;

		/*if (strncmp(command, "run ", 4) == 0)
		{
		printf("got run command");
		int len = strlen(command) - 4;
		memmove(command, command + 4, len);
		command[len] = 0;
		}*/

		int i = 0;
		while (command[i] != ' ' && command[i] != 0)
			i++;

		std::string args;
		if (command[i] && command[i + 1])
			args = (const char*)&command[i + 1];
		command[i] = 0;

		if (strcmp(command, "runtests") == 0)
		{
			//finish tests
			OptionParser parser;
			parser.AddOption("o", "0");
			parser.AddOption("f", "1");
			parser.Parse(args);

			CompilerOptions options;
			options.optimization = parser.GetOption("o").GetInt();
			options.force = parser.GetOption("f").GetString().length() == 0;
			std::string config = "";
			if (parser.commands.size())
				config = parser.commands.front();

			std::vector<char*> programs = { "Namespaces", "Inheritance", "ExtensionMethods", "Generators", "IfStatements", "Unions", "ForLoop", "OperatorPrecedence", "DefaultConstructors" };

			for (auto ii : programs)
			{
				printf("Running test '%s'...\n", ii);

				//add options to this later
				auto project = JetProject::Load("tests/" + std::string(ii));

				if (!project)
				{
					printf("Test project %s not found\n", ii);
					continue;
				}

				DiagnosticBuilder b([](Diagnostic& x) {x.Print(); });
				auto compilation = Compilation::Make(project, &b);

				std::string o;
				for (auto ii : compilation->asts)
				{
					ii.second->Print(o, compilation->sources[ii.first]);
					//std::cout << o;

					//check that they match!!!
					if (strcmp(o.c_str(), compilation->sources[ii.first]->GetLinePointer(1)) != 0)
					{
						printf("Tree printing test failed, did not match original\n");
						std::cout << o;
					}

					o.clear();
				}

				if (b.GetErrors().size() > 0)
				{
					printf("Test '%s' failed\n", ii);
				}
				else
				{
					//assemble and execute, look for pass or fail
					compilation->Assemble(0);

					//
					std::string cmd = "tests\\" + std::string(ii) + "\\build\\" + std::string(ii) + ".exe";
					auto res = exec(cmd.c_str());
					printf("%s\n", res.c_str());

					if (res.find("fail") != -1)
						printf("Test '%s' failed\n", ii);
					//need to figure out why nothing is being printed
				}

				printf("\n");

				delete compilation;
				delete project;
			}
			continue;
		}

		OptionParser parser;
		parser.AddOption("o", "0");
		parser.AddOption("f", "0");
		parser.Parse(args);

		//add options to this later
		CompilerOptions options;
		options.optimization = parser.GetOption("o").GetInt();
		options.force = parser.GetOption("f").GetString().length() == 0;
		std::string config = "";
		if (parser.commands.size())
			config = parser.commands.front();
		c2.Compile(command, &options, config, &parser);
	}

	return 0;
}

