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

//#include "Parser.h"
//#include "Lexer.h"

#include <llvm\IR\IRBuilder.h>
#include <llvm\IR\LLVMContext.h>
#include <llvm\IR\Module.h>

//#include <string>
//#include <vector>
//#include <map>

#include "Types.h"
#include "Token.h"

#include "llvm/ExecutionEngine/ExecutionEngine.h"
//#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Transforms/Scalar.h"


//lets get functions with the same name but different args working for structs
//then add templates/generics
namespace Jet
{
	//void Error(const std::string& msg, Token token);
	void ParserError(const std::string& msg, Token token);//todo

	class BlockExpression;

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

	//add global variables
	//global compiler context

	struct JetError
	{
		std::string message;
		std::string file;

		std::string line;//the line of code that the function was on
		Token token;
		Token end;//optional

		void Print();
	};

	class CompilerContext;
	class JetProject;
	class Compilation
	{
		friend class Type;
		friend class Struct;
		friend class Function;
		friend class Compiler;
		friend class Expression;
		friend class CompilerContext;
		friend class FunctionExpression;
		friend class ExternExpression;
		friend class TraitExpression;
		friend class LocalExpression;

		llvm::TargetMachine* target;
		llvm::LLVMContext& context;
		
		struct DebugInfo {
			llvm::DICompileUnit cu;
			llvm::DIFile file;
		} debug_info;

		std::map<std::string, Trait*> traits;

		CompilerContext* current_function;

		//int errors;

		Compilation(JetProject* proj);
	public:
		//dont use these k
		llvm::Module* module;
		llvm::DIBuilder* debug;
		llvm::IRBuilder<> builder;

		//these are some of the basic types
		Type* DoubleType;
		Type* IntType;
		Type* BoolType;

		JetProject* project;

		std::map<std::string, Source*> sources;
		std::map<std::string, BlockExpression*> asts;

		std::multimap<std::string, Function*> functions;

		~Compilation();

		//get array types inside of structs working
		Type* AdvanceTypeLookup(const std::string& name);

		std::map<std::string, Type*> types;
		Type* LookupType(const std::string& name);

		std::map<std::string, CValue> globals;
		CValue AddGlobal(const std::string& name, Type* t);

		static Compilation* Make(JetProject* proj);

		std::vector<JetError> errors;
		std::vector<JetError>& GetErrors()
		{
			return this->errors;
		}

		void Error(const std::string& string, Token token);

		//generates and outputs an exe or lib file
		void Assemble(int olevel);

	private:

		void Optimize(int level);

		void OutputIR(const char* filename);
		void OutputPackage(const std::string& project_name, int o_level);

		void Dump()
		{
			//if (module)
			//module->dump();
		}

		void SetTarget();//make this customizable later
	};

	class Compiler
	{
	public:

		//returns if was successful
		bool Compile(const char* projectfile, CompilerOptions* options = 0);

		
	};
};
#endif