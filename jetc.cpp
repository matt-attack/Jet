
/*#ifdef _DEBUG
#ifndef DBG_NEW
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#define new DBG_NEW
#endif

#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif*/

#include <stdio.h>

//typedef wchar_t     _TCHAR;

#include <stack>
#include <vector>

#define CODE(code) #code

#include "Compiler.h"

#include <iostream>
#include <string>
#include <fstream>
#include <streambuf>
#include <math.h>
#ifdef _WIN32
#include <io.h>
#include <tchar.h>
#include <Windows.h>
#endif
#include <functional>

using namespace Jet;


/*#pragma comment (lib, "LLVMSupport.lib")
#pragma comment (lib, "LLVMCore.lib")
#pragma comment (lib, "LLVMInstCombine.lib")
#pragma comment (lib, "LLVMScalarOpts.lib")
#pragma comment (lib, "LLVMTransformUtils.lib")

#pragma comment (lib, "LLVMTarget.lib")
#pragma comment (lib, "LLVMAnalysis.lib")
#pragma comment (lib, "LLVMMC.lib")

#pragma comment (lib, "LLVMExecutionEngine.lib")
#pragma comment (lib, "LLVMRuntimeDyld.lib")
#pragma comment (lib, "LLVMObject.lib")
#pragma comment (lib, "LLVMMCJIT.lib")
#pragma comment (lib, "LLVMMCParser.lib")
#pragma comment (lib, "LLVMMCDisassembler.lib")
#pragma comment (lib, "LLVMipa.lib")
#pragma comment (lib, "LLVMBitReader.lib")
#pragma comment (lib, "LLVMX86CodeGen.lib")
#pragma comment (lib, "LLVMX86Info.lib")
#pragma comment (lib, "LLVMX86Desc.lib")
#pragma comment (lib, "LLVMX86Utils.lib")
#pragma comment (lib, "LLVMX86AsmParser.lib")
#pragma comment (lib, "LLVMX86AsmPrinter.lib")
#pragma comment (lib, "LLVMSelectionDAG.lib")
#pragma comment (lib, "LLVMCodeGen.lib")
#pragma comment (lib, "LLVMAsmPrinter.lib")
#pragma comment (lib, "LLVMAsmParser.lib")*/


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

