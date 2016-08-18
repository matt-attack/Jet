#ifndef JET_COMPILATION_HEADER
#define JET_COMPILATION_HEADER

#include <string>
#include <map>
#include <vector>
#include <functional>

#include "Token.h"
#include "Types/Types.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

namespace llvm
{
	class DIBuilder;
}
namespace Jet
{
	std::string exec(const char* cmd);

	struct Diagnostic
	{
		std::string message;
		std::string file;

		std::string line;//the line of code that the function was on
		Token token;
		Token end;//optional

		int severity;//0 = error, 1 = warning, 2 = info ect..
		void Print();
	};

	class DiagnosticBuilder
	{
		std::vector<Diagnostic> diagnostics;
		std::function<void(Diagnostic&)> callback;
	public:

		DiagnosticBuilder(std::function<void(Diagnostic&)> callback) : callback(callback)
		{

		}

		void Error(const std::string& text, const Token& token);

		std::vector<Diagnostic>& GetErrors()
		{
			return diagnostics;
		}
	};

	class Namespace;
	class Source;
	class Type;
	class Trait;
	class CompilerContext;
	class JetProject;
	class BlockExpression;
	class Compilation
	{
		friend class Type;
		friend struct Struct;
		friend class Function;
		friend class Compiler;
		friend class Expression;
		friend class CompilerContext;
		friend class NewExpression;
		friend class FreeExpression;
		friend class FunctionExpression;
		friend class ExternExpression;
		friend class TraitExpression;
		friend class LocalExpression;
		friend class StructExpression;

		llvm::TargetMachine* target;
		llvm::LLVMContext& context;

		struct DebugInfo {
			llvm::DICompileUnit* cu;
			llvm::DIFile* file;
		} debug_info;

		std::vector<std::pair<Namespace*, Type**>> types;//a list of all referenced types and their locations, used for type lookahead

		std::map<std::string, Trait*> traits;//a list of all traits
		std::vector<Function*> functions;//a list of all functions to be able to optimize and shizzle

		//list of not fully compiled templates to finish before completion
		std::vector<Type*> unfinished_templates;

		std::map<int, Type*> function_types;//a cache of function types

		DiagnosticBuilder* diagnostics;

		CompilerContext* current_function;

		Namespace* global;

		Compilation(JetProject* proj);
	public:
		bool typecheck;
		bool compiling_includes = false;

		//dont use these k
		llvm::Module* module;
		llvm::DIBuilder* debug;
		llvm::IRBuilder<> builder;

		//these are some of the basic types
		Type* DoubleType;
		Type* IntType;
		Type* BoolType;

		JetProject* project;

		Namespace* ns;

		std::map<std::string, Source*> sources;
		std::map<std::string, BlockExpression*> asts;

		~Compilation();

		void AdvanceTypeLookup(Type** dest, const std::string& name, Token* location);


		Type* LookupType(const std::string& name, bool load = true);
		Type* TryLookupType(const std::string& name);

		CValue AddGlobal(const std::string& name, Type* t, llvm::Constant* init = 0, bool intern = false);

		static Compilation* Make(JetProject* proj, DiagnosticBuilder* builder, bool time = false);

		std::vector<Diagnostic>& GetErrors()
		{
			return this->diagnostics->GetErrors();// errors;
		}

		void Error(const std::string& string, Token token);

		//generates and outputs an exe or lib file
		void Assemble(int olevel, bool time = false);


		//racer stuff
		Function* GetFunctionAtPoint(const char* file, int line);

	private:

		Type* GetFunctionType(Type* return_type, const std::vector<Type*>& args);

		Function* GetFunction(const std::string& name, const std::vector<CValue>& args)
		{
			return this->GetFunction(name);
		}

		Function* GetFunction(const std::string& name, const std::vector<Type*>& args)
		{
			return this->GetFunction(name);
		}


		Function* GetFunction(const std::string& name);
		Symbol GetVariableOrFunction(const std::string& name);

		void ResolveTypes();

		void Optimize(int level);

		void OutputIR(const char* filename);
		void OutputPackage(const std::string& project_name, int o_level, bool time = false);

		void Dump()
		{
			if (module)
				module->dump();
		}

		void SetTarget();//make this customizable later
	};

}
#endif