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

	//add global variables
	//global compiler context
	class CompilerContext;
	class Compiler
	{

	public:
		llvm::IRBuilder<> builder;
		llvm::LLVMContext& context;
		llvm::Module* module;

		std::multimap<std::string, Function*> functions;
		std::map<std::string, Trait*> traits;

		CompilerContext* current_function;

		Compiler() : builder(llvm::getGlobalContext()), context(llvm::getGlobalContext())
		{
			//insert basic types
			types["double"] = new Type(Types::Double);
			types["float"] = &DoubleType;// new Type(Types::Float);
			types["int"] = &IntType;// new Type(Types::Int);
			types["short"] = new Type(Types::Short);
			types["char"] = new Type(Types::Char);
			types["bool"] = &BoolType;// new Type(Types::Bool);
			types["void"] = &VoidType;// new Type(Types::Void);
		}

		//returns paths to dependencies
		std::vector<std::string> Compile(const char* projectfile);

		void Compile(const char* code, const char* filename);

		void Optimize();

		void OutputIR(const char* filename);
		void OutputPackage(const std::string& project_name);

		void Dump()
		{
			if (module)
				module->dump();
		}

		//get array types inside of structs working
		Type* AdvanceTypeLookup(const std::string& name)
		{
			auto type = types.find(name);
			if (type == types.end())
			{
				//create it, its not a basic type, todo later
				if (name[name.length() - 1] == '*')
				{
					//its a pointer
					auto t = types[name.substr(0, name.length() - 1)];

					Type* type = new Type;
					type->base = t;
					type->type = Types::Pointer;

					types[name] = type;
					return type;
				}
				else if (name[name.length() - 1] == ']')
				{
					//its an array
					int p = 0;
					for (p = 0; p < name.length(); p++)
						if (name[p] == '[')
							break;

					auto len = name.substr(p + 1, name.length() - p - 2);

					auto tname = name.substr(0, p);
					auto t = this->LookupType(tname);

					Type* type = new Type;
					type->base = t;
					type->type = Types::Array;
					type->size = std::stoi(len);//cheat for now
					types[name] = type;
					return type;
				}

				//who knows what type it is, create a dummy one
				Type* type = new Type;
				type->type = Types::Invalid;
				type->data = 0;
				types[name] = type;

				return type;
			}
			return type->second;
		}

		std::map<std::string, Type*> types;
		Type* LookupType(const std::string& name);
	};
};
#endif