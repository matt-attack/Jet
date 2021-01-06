
#define _CRTDBG_MAP_ALLOC

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

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

using namespace Jet;


#include <sstream>
//#include <clang-c/Index.h>

#include "OptionParser.h"

#ifdef _DEBUG
#ifndef DBG_NEW
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#define new DBG_NEW
#endif
#endif  // _DEBUG

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
	"uchar",
	"uchar",//5
	"short",//char16
	"int",//char32
	"ushort",//ushort
	"uint",//unint
	"ulong",//10 ulong
	"ulong",//uint128
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
/*std::string convert_type(CXType type)
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
	//if it makes it here its probably a function pointer 
	out = tstr;

	//also handle[] as a *
	int pos = 0;
	while ((pos = out.find("const ")) != -1)
		out.erase(pos, 6);

	//int pos = 0;
	while ((pos = out.find("[]")) != -1)
		out.replace(pos, 2, "*");

	//look for a const in the name and remove it


	clang_disposeString(tname);

	return out;
}*/
//todo: C++ bindings
//ok integrate this, then we need to add unsigned types
std::string generate_jet_from_header(const char* header)
{
	return "";
	/*CXIndex index = clang_createIndex(0, 0);
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
		//ok, lets generate c++ bindings
		//	c.kind == CXCursor_ClassDecl
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
			printf("Warning: exporting variables such as '%s' not yet supported\n", str);
			clang_disposeString(name);

			auto type = clang_getCursorType(c);
			//we can handle this later
			//throw 7;
			return CXChildVisit_Continue;
		}
		return CXChildVisit_Continue;
	}, &data);

	while (data.struct_stack.size() > 0)
	{
		data.struct_stack.pop();

		//print out last part of structure 
		out += "}\n";
	}

	if (tu)
		clang_disposeTranslationUnit(tu);
	clang_disposeIndex(index);

	return out;*/
}

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

void DoCommand(int argc, char* argv[])
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
		return;
	}
	else if (cmd == "runtests")
	{
		OptionParser parser;
		SetupDefaultCommandOptions(&parser);
		parser.Parse(argc, argv);

		//finish tests
		parser.GetOption("f").SetValue("1");

		CompilerOptions options;
		options.ApplyOptions(&parser);

		std::string config = "";
		if (parser.commands.size())
			config = parser.commands.front();

		std::vector<char*> programs = { "OperatorOverloads", "SmartPointerTest", "Globals", "NewFree", "Namespaces", "Inheritance", "ExtensionMethods", "Generators", "IfStatements", "Unions", "ForLoop", "OperatorPrecedence", "DefaultConstructors", "Enums" };


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

			MakeDocs(compilation);

			//also need to output to file and integrate this correctly
			//	also need to implement it for classes
			if (compilation == 0)
			{
				delete project;
				return;
			}
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
				printf("Test '%s' failed to build\n", ii);
			}
			else
			{
				//assemble and execute, look for pass or fail
				compilation->Assemble();

				//
				std::string cmd = "tests\\" + std::string(ii) + "\\build\\" + std::string(ii) + ".exe";
				auto res = exec(cmd.c_str());
				printf("%s\n", res.c_str());

				if (res.find("fail") != -1)
					printf("Test '%s' failed in execution\n", ii);
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
			std::ofstream o(two + ".jet");
			o << str;
			o.close();
		}
		return;
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
		return;
	}
	else if (cmd == "build")
	{
		OptionParser parser;
		SetupDefaultCommandOptions(&parser);
		parser.Parse(argc-1, &argv[1]);

		CompilerOptions options;
		options.ApplyOptions(&parser);

		std::string config = "";
		if (parser.commands.size() > 1)
			config = parser.commands[1];

		Jet::Compiler c;
		if (argc >= 3)
			c.Compile(argv[2], &options, config, &parser);
		else
			c.Compile("", &options, config, &parser);
	}
	else
	{
		printf("Unknown verb.\n");
	}
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
int main(int argc, char* argv[])
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
		DoCommand(argc, argv);

		return 0;
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
#ifdef _WIN32
			//_CrtDumpMemoryLeaks();
#endif
			continue;
		}
	}

	return 0;
}

