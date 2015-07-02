
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

class OptionVar
{
	const char* name;
	std::string value;
public:
	OptionVar()
	{

	}

	OptionVar(const char* name, const char* def)
	{
		this->name = name;
		this->value = def;
	}

	bool GetBool()
	{
		return false;
	}

	int GetInt()
	{
		if (this->value.length() == 0)
			return 0;
		try
		{
			return std::stoi(this->value);
		}
		catch (...)
		{
			return 0;
		}
	}

	std::string GetString()
	{
		return value;
	}

	void SetValue(const std::string& value)
	{
		this->value = value;
	}
};

class OptionParser
{

	std::map<std::string, OptionVar> vars;
public:

	OptionVar& GetOption(const char* name)
	{
		return vars[name];
	}

	void AddOption(const char* name, const char* def)
	{
		vars[name] = OptionVar(name, def);
	}

	void Parse(const std::string& args)
	{
		int pos = 0;
		while (pos < args.length())
		{
			char c = args[pos++];
			if (c == '-' && pos < args.length())//its an option
			{
				char o = args[pos++];
				std::string option;
				option += o;

				//read in value
				std::string value;
				while (args[pos] && args[pos] != ' ')
					value += args[pos++];
				
				auto find = vars.find(option);
				if (find != vars.end())
					vars[option].SetValue(value);
			}
		}
	}
};

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
		std::cin.getline(command, 800);

		Jet::Compiler c2;

		if (strncmp(command, "run ", 4) == 0)
		{
			printf("got run command");
			int len = strlen(command) - 4;
			memmove(command, command + 4, len);
			command[len] = 0;
		}
		
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
		c2.Compile(command, &options);
	}

	return 0;
}

