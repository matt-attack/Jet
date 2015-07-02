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
	void Error(const std::string& msg, Token token);
	void ParserError(const std::string& msg, Token token);

	class BlockExpression;

	struct CValue
	{
		//my type info
		Type* type;
		llvm::Value* val;

		CValue()
		{
			type = &VoidType;
			val = 0;
		}

		CValue(Type* type, llvm::Value* val) : type(type), val(val) {}
	};

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
	class CompilerContext;
	class Compiler
	{
		llvm::TargetMachine* target;
	public:
		llvm::IRBuilder<> builder;
		llvm::LLVMContext& context;
		llvm::Module* module;

		std::multimap<std::string, Function*> functions;
		std::map<std::string, Trait*> traits;

		CompilerContext* current_function;

		//these are some of the basic types
		Type* DoubleType;
		Type* IntType;
		Type* BoolType;

		Compiler() : builder(llvm::getGlobalContext()), context(llvm::getGlobalContext())
		{
			this->target = 0;

			//insert basic types
			types["float"] = new Type("float", Types::Float);
			this->DoubleType = new Type("double", Types::Double);
			types["double"] = this->DoubleType;// new Type("double", Types::Double);// &DoubleType;// new Type(Types::Float);
			types["long"] = new Type("long", Types::Long);
			this->IntType = new Type("int", Types::Int);
			types["int"] = this->IntType;// new Type("int", Types::Int);// &IntType;// new Type(Types::Int);
			types["short"] = new Type("short", Types::Short);
			types["char"] = new Type("char", Types::Char);
			this->BoolType = new Type("bool", Types::Bool);// &BoolType;// new Type(Types::Bool);
			types["bool"] = this->BoolType;
			types["void"] = new Type("void", Types::Void);// &VoidType;// new Type(Types::Void);
		}

		~Compiler();

		//returns paths to dependencies
		std::vector<std::string> Compile(const char* projectfile, CompilerOptions* options = 0);

		void Optimize();

		void OutputIR(const char* filename);
		void OutputPackage(const std::string& project_name);

		void Dump()
		{
			if (module)
				module->dump();
		}

		void SetTarget();//make this customizable later

		//get array types inside of structs working
		Type* AdvanceTypeLookup(const std::string& name);

		Trait* AdvanceTraitLookup(const std::string& trait);

		std::map<std::string, Type*> types;
		Type* LookupType(const std::string& name);
		
		std::map<std::string, CValue> globals;
		CValue AddGlobal(const std::string& name, Type* t);
	};
};
#endif