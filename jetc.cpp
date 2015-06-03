
/*#ifdef _DEBUG
#ifndef DBG_NEW
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#define new DBG_NEW
#endif

#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif*/

#include <stdio.h>

#include <stack>
#include <vector>

#define CODE(code) #code

#include "Compiler.h"

#include <iostream>
#include <string>
#include <fstream>
#include <streambuf>
#include <math.h>
#include <functional>

using namespace Jet;


#include <sstream>

int main(int argc, char* argv[])
{
	if (argc > 1)//if we get a command, just try and build the project at that path
	{
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
		char arg[150]; char command2[150];
		memset(arg, 0, 150);
		memset(command2, 0, 150);
		std::cin.getline(command, 800);

		Jet::Compiler c2;

		//add options to this later
		c2.Compile(command);
	}

	return 0;
}

