
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
#include <clang-c/Index.h>

#include "OptionParser.h"

/*  CXType_Void = 2,
CXType_Bool = 3,
CXType_Char_U = 4,
CXType_UChar = 5,
CXType_Char16 = 6,
CXType_Char32 = 7,
CXType_UShort = 8,
CXType_UInt = 9,
CXType_ULong = 10,
CXType_ULongLong = 11,
CXType_UInt128 = 12,
CXType_Char_S = 13,
CXType_SChar = 14,
CXType_WChar = 15,
CXType_Short = 16,
CXType_Int = 17,
CXType_Long = 18,
CXType_LongLong = 19,
CXType_Int128 = 20,
CXType_Float = 21,
CXType_Double = 22,
CXType_LongDouble = 23,*/
const char* type_conversion[] =
{
	"", "",
	"void",//2
	"int",
	"char",
	"char",//5
	"char",
	"char",
	"short",//ushort
	"int",//unint
	"long",//10 ulong
	"long",//uint128
	"",
	"char",//char_s
	"char",
	"",//15
	"short",//short
	"int",//int
	"long",//long
	"long",//long long
	"",//20 int128
	"float",//float
	"double",//double
	"double",//long double
};
//clang_getPointeeType
std::string convert_type(CXType type)
{
	std::string out;
	CXString tname = clang_getTypeSpelling(type);
	if (type.kind == CXType_Pointer)
	{
		auto pt = clang_getPointeeType(type);
		out += convert_type(pt);
		out += '*';
		return out;
	}
	else if (type.kind == CXType_ConstantArray)
	{
		auto pt = clang_getArrayElementType(type);
		auto size = clang_getArraySize(type);
		out += convert_type(pt);
		out += '[';
		out += std::to_string(size);
		out += ']';
		return out;
	}
	else if (type.kind == CXType_Enum)
	{
		printf("");
	}
	else if ((int)type.kind >= 2 && (int)type.kind < 24)
	{
		//use conversion table
		auto res = type_conversion[(int)type.kind];
		if (res[0] == 0)
			return res;
		return res;
	}

	if (clang_isConstQualifiedType(type))
	{
		auto tstr = clang_getCString(tname);
		//ok, lets remove the const
		if (strncmp(tstr, "const ", 6) == 0)
		{
			out = &tstr[6];
		}
		else
			throw 7;
		clang_disposeString(tname);

		return out;
	}

	if (type.kind == CXType_Typedef)
	{
		auto tstr = clang_getCString(tname);
		out = tstr;
		clang_disposeString(tname);

		return out;
	}

	auto tstr = clang_getCString(tname);

	if (strncmp(tstr, "struct ", 7) == 0)
	{
		//its a struct
		out = &tstr[7];

		clang_disposeString(tname);

		return out;
	}
	else if (strncmp(tstr, "union ", 6) == 0)
	{
		//its a union
		out = &tstr[6];

		clang_disposeString(tname);

		return out;
	}
	else if (strncmp(tstr, "enum ", 5) == 0)
	{
		//its a enum
		out = &tstr[5];

		clang_disposeString(tname);

		return out;
	}

	out = tstr;

	clang_disposeString(tname);

	return out;
}
//ok integrate this, then we need to add unsigned types
std::string generate_jet_from_header(const char* header)
{
	CXIndex index = clang_createIndex(0, 0);
	const char *args[] = {
		"-I\"C:/Program Files (x86)/Microsoft Visual Studio 12.0/VC/include\""
		//"-I/usr/include",
		//"-I."
	};
	int numArgs = sizeof(args) / sizeof(*args);
	//ok... lets write a dummy file then delete it
	//this is terrible...
	std::ofstream tempf("_hdrgentmp.c");
	tempf << "#include \"";
	tempf << header;
	tempf << "\"\n";
	tempf.close();

	//"C:/users/Matthew/Desktop/main.c"
	CXTranslationUnit tu = clang_createTranslationUnitFromSourceFile(index, "_hdrgentmp.c", numArgs, args, NULL, 0);
	if (tu == 0)
		printf("file not found\n");

	unsigned diagnosticCount = clang_getNumDiagnostics(tu);
	for (unsigned i = 0; i < diagnosticCount; i++)
	{
		CXDiagnostic diagnostic = clang_getDiagnostic(tu, i);
		CXString text = clang_getDiagnosticSpelling(diagnostic);
		auto str = clang_getCString(text);
		clang_disposeString(text);
		clang_disposeDiagnostic(diagnostic);
	}
	//walk the tree
	auto cursor = clang_getTranslationUnitCursor(tu);

	struct cl_data
	{
		std::string* out;
		//then a stack of stuff
		std::stack<CXCursor> struct_stack;// list of current struct parents
	} data;
	std::string out;
	data.out = &out;
	clang_visitChildren(cursor, [](CXCursor c, CXCursor parent, CXClientData client_data)
	{
		cl_data* data = (cl_data*)client_data;
		std::string* out = data->out;
		if (data->struct_stack.size() > 0 && clang_equalCursors(parent, data->struct_stack.top()) == 0)
		{
			data->struct_stack.pop();

			//print out last part of structure 
			*out += "}\n";
		}
		if (c.kind == CXCursor_StructDecl)
		{
			CXString name = clang_getCursorSpelling(c);
			auto str = clang_getCString(name);
			//printf("Struct: %s\n", str);

			*out += "struct ";
			*out += str;
			*out += "\n{\n";

			clang_disposeString(name);

			//push the structure
			data->struct_stack.push(c);

			//ok get rid of this and move it down
			/*clang_visitChildren(c, [](CXCursor c, CXCursor parent, CXClientData client_data)
			{
			std::string* out = (std::string*)client_data;

			if (c.kind == CXCursor_FieldDecl)
			{
			CXString name = clang_getCursorSpelling(c);
			auto str = clang_getCString(name);
			//printf("    Field: %s ", str);

			auto type = clang_getCursorType(c);

			*out += convert_type(type);
			*out += ' ';
			*out += str;
			*out += ";\n";
			clang_disposeString(name);

			return CXChildVisit_Continue;
			}
			//oops, we can have struct and enum declarations in a struct todo
			//	also there are "first attributes" whatever those are
			return CXChildVisit_Continue;
			}, out);*/

			//*out += "}\n";
			return CXChildVisit_Recurse;
		}
		else if (c.kind == CXCursor_FieldDecl)
		{
			CXString name = clang_getCursorSpelling(c);
			auto str = clang_getCString(name);
			//printf("    Field: %s ", str);

			auto type = clang_getCursorType(c);

			*out += convert_type(type);
			*out += ' ';
			*out += str;
			*out += ";\n";
			clang_disposeString(name);

			return CXChildVisit_Continue;
		}
		else if (c.kind == CXCursor_EnumDecl)
		{
			CXString name = clang_getCursorSpelling(c);
			auto str = clang_getCString(name);
			//printf("Struct: %s\n", str);

			*out += "enum ";
			*out += str;
			*out += "\n{\n";

			clang_disposeString(name);

			clang_visitChildren(c, [](CXCursor c, CXCursor parent, CXClientData client_data)
			{
				std::string* out = (std::string*)client_data;

				if (c.kind == CXCursor_EnumConstantDecl)
				{
					CXString name = clang_getCursorSpelling(c);
					auto str = clang_getCString(name);
					//printf("    Field: %s ", str);

					auto value = clang_getEnumConstantDeclValue(c);

					*out += str;// convert_type(type);
					*out += " = ";
					*out += std::to_string(value);
					*out += ",\n";
					clang_disposeString(name);

					return CXChildVisit_Continue;
				}

				return CXChildVisit_Continue;
			}, out);

			(*out).pop_back();
			(*out).pop_back();

			*out += "\n}\n";

			return CXChildVisit_Continue;
		}
		else if (c.kind == CXCursor_FunctionDecl)
		{
			CXString name = clang_getCursorSpelling(c);
			auto str = clang_getCString(name);
			//printf("Function: %s\n", str);

			auto type = clang_getCursorType(c);

			auto return_type = clang_getResultType(type);
			auto calling_conv = clang_getFunctionTypeCallingConv(type);

			*out += "extern fun ";
			*out += convert_type(return_type);// tstr;
			*out += ' ';
			*out += str;
			*out += '(';

			clang_disposeString(name);

			int num_args = clang_Cursor_getNumArguments(c);
			for (int i = 0; i < num_args; i++)
			{
				auto arg = clang_Cursor_getArgument(c, i);

				auto type = clang_getCursorType(arg);
				//CXString tname = clang_getTypeSpelling(type);
				//auto str = clang_getCString(tname);
				//printf("arg %s \n", str);
				*out += convert_type(type);
				//clang_disposeString(tname);

				//CXString name = clang_getCursorSpelling(arg);
				//auto str = clang_getCString(name);
				//printf("arg %s \n", str);
				*out += " arg" + std::to_string(i);

				//clang_disposeString(name);

				if (i < num_args - 1)
					*out += ',';
			}

			*out += ");\n";

			return CXChildVisit_Continue;
		}
		else if (c.kind == CXCursor_TypedefDecl)
		{
			CXString name = clang_getCursorSpelling(c);
			auto str = clang_getCString(name);
			//printf("Typedef: %s ", str);

			auto type = clang_getTypedefDeclUnderlyingType(c);

			//output jet stuff
			//format is typedef RANTPATTERN = char*;
			*out += "typedef ";
			*out += str;
			*out += " = ";
			*out += convert_type(type);
			*out += ";\n";

			clang_disposeString(name);

			return CXChildVisit_Continue;
		}
		else if (c.kind == CXCursor_VarDecl)
		{
			CXString name = clang_getCursorSpelling(c);
			auto str = clang_getCString(name);
			printf("Var: %s\n", str);
			clang_disposeString(name);

			auto type = clang_getCursorType(c);
			//we can handle this later
			//throw 7;
			return CXChildVisit_Continue;
		}
		return CXChildVisit_Continue;
	}, &data);

	if (tu)
		clang_disposeTranslationUnit(tu);
	clang_disposeIndex(index);

	return out;
}

void DoCommand(int argc, char* argv[])
{
	OptionParser parser;
	parser.AddOption("o", "0");
	parser.AddOption("f", "0");
	parser.Parse(argc, argv);

	std::string cmd = argc > 1 ? argv[1] : "";
	if (cmd == "runtests")
	{
		//finish tests
		OptionParser parser;
		parser.AddOption("o", "0");
		parser.AddOption("f", "1");
		parser.Parse(argc, argv);

		CompilerOptions options;
		options.optimization = parser.GetOption("o").GetInt();
		options.force = parser.GetOption("f").GetString().length() == 0;
		std::string config = "";
		if (parser.commands.size())
			config = parser.commands.front();

		std::vector<char*> programs = { "Globals", "NewFree", "Namespaces", "Inheritance", "ExtensionMethods", "Generators", "IfStatements", "Unions", "ForLoop", "OperatorPrecedence", "DefaultConstructors", "Enums" };


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
		return;
	}
	else if (cmd == "convert")
	{
		std::string two = argv[2];
		//fix conversion of attributes for calling convention and fix function pointers
		std::string str = generate_jet_from_header(two.c_str());
		if (str.length() == 0)
			printf("No such file.");
		else
		{
			std::ofstream o(two+".jet");
			o << str;
			o.close();
		}
		return;
	}

	//add options to this later
	CompilerOptions options;
	options.optimization = parser.GetOption("o").GetInt();
	options.force = parser.GetOption("f").GetString().length() == 0;
	std::string config = "";
	if (parser.commands.size())
		config = parser.commands.front();

	//todo, use command list from parser and get configurations working correctly
	Jet::Compiler c;
	if (strcmp(argv[1], "-build") == 0)
		c.Compile("", &options, config, &parser);
	else
		c.Compile(argv[1], &options, config, &parser);
}

int main(int argc, char* argv[])
{
	if (argc > 1)//if we get a command, just try and build the project at that path
	{
		DoCommand(argc, argv);

		return 0;
	}

	printf("Input the path to a project folder to build it\n");
	while (true)
	{
		printf("\n>");
		char command[800];
		std::cin.getline(command, 800);

		//split the string
		{
			
			char* args[400] = {};
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

			continue;
		}

		//Jet::Compiler c2;

		/*if (strncmp(command, "run ", 4) == 0)
		{
		printf("got run command");
		int len = strlen(command) - 4;
		memmove(command, command + 4, len);
		command[len] = 0;
		}*/

		/*int i = 0;
		while (command[i] != ' ' && command[i] != 0)
			i++;

		std::string args;
		if (command[i] && command[i + 1])
			args = (const char*)&command[i + 1];
		//command[i] = 0;


		std::stringstream commands(command);
		std::string cmd;
		commands >> cmd;
		if (cmd == "runtests")// strcmp(command, "runtests") == 0)
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

			std::vector<char*> programs = { "Globals", "NewFree", "Namespaces", "Inheritance", "ExtensionMethods", "Generators", "IfStatements", "Unions", "ForLoop", "OperatorPrecedence", "DefaultConstructors", "Enums" };


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
		else if (cmd == "convert")
		{
			std::stringstream commands(command);
			std::string one, two;
			commands >> one;
			commands >> two;
			std::string str = generate_jet_from_header(two.c_str());
			if (str.length() == 0)
				printf("No such file.");
			else
			{
				std::ofstream o("windows.jet");
				o << str;
				o.close();
			}
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
		c2.Compile(cmd.c_str(), &options, config, &parser);*/
	}

	return 0;
}

