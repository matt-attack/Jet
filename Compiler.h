#ifndef JET_COMPILER_HEADER
#define JET_COMPILER_HEADER

/*#ifdef _DEBUG
#ifndef DBG_NEW
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#define new DBG_NEW
#endif

#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif*/

#include "Types/Types.h"
#include "Token.h"
#include "Compilation.h"

//#include "llvm/ExecutionEngine/ExecutionEngine.h"
//#include "llvm/ExecutionEngine/MCJIT.h"
//#include "llvm/ExecutionEngine/SectionMemoryManager.h"
//#include "llvm/IR/LegacyPassManager.h"
//#include "llvm/Analysis/Passes.h"
//#include "llvm/Support/TargetSelect.h"
//#include "llvm/Transforms/Scalar.h"


class OptionParser;

namespace Jet
{
	void ParserError(const std::string& msg, Token token);//todo

	struct CompilerOptions
	{
		int optimization;//from 0-3
		bool force;
		CompilerOptions()
		{
			this->force = false;
			this->optimization = 0;
		}
	};

	class Compiler
	{
	public:

		//returns if was successful
		bool Compile(const char* projectfile, CompilerOptions* options = 0, const std::string& config = "", OptionParser* parser = 0);
	};
};

#endif