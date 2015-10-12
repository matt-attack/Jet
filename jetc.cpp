
/*#ifdef _DEBUG
#ifndef DBG_NEW
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#define new DBG_NEW
#endif

#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif*/

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
		if (command[i] && command[i+1])
			args = (const char*)&command[i+1];
		command[i] = 0;

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

