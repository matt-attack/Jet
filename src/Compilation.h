#ifndef JET_COMPILATION_HEADER
#define JET_COMPILATION_HEADER

#include <string>
#include <map>
#include <vector>
#include <functional>

#include "Token.h"
#include "types/Types.h"
#include "types/Namespace.h"

#include "Diagnostics.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

namespace llvm
{
	class DIBuilder;
	class TargetMachine;
}
namespace Jet
{
	std::string exec(const char* cmd, int* error_code = 0);

	struct Namespace;
	class Source;
	class Type;
	struct Trait;
	class CompilerContext;
	class JetProject;
	class BlockExpression;
	class Compilation
	{
		friend class Type;
		friend struct Struct;
		friend struct Function;
		friend class Compiler;
		friend class Expression;
		friend class CompilerContext;
		friend class NewExpression;
		friend class FreeExpression;
		friend class FunctionExpression;
		friend class ExternExpression;
		friend class TraitExpression;
		friend class LetExpression;
		friend class StructExpression;

		llvm::TargetMachine* target;
		llvm::LLVMContext context;

		struct DebugInfo {
			llvm::DICompileUnit* cu;
			llvm::DIFile* file;
		} debug_info;

		std::vector<std::pair<Namespace*, Type**>> unresolved_types;//a list of all referenced types and their locations, used for type lookahead

		std::map<std::string, Trait*> traits;//a list of all traits
		std::vector<Function*> functions;//a list of all functions to be able to optimize and shizzle

		//list of not fully compiled templates to finish before completion
		std::vector<Type*> unfinished_templates;

		std::multimap<int, Type*> function_types;//a cache of function types

		DiagnosticBuilder* diagnostics;

		CompilerContext* current_function;// the current function being compiled

        std::string real_target;

		Namespace* global;

		Compilation(const JetProject* proj);

        int pointer_size = 4;

	public:
		bool typecheck;
		bool compiling_includes = false;

		//dont use these k
		llvm::Module* module;
		llvm::DIBuilder* debug;
		llvm::IRBuilder<> builder;

		//these are some of the basic types
		Type* DoubleType;
		Type* FloatType;
		Type* IntType;
		Type* BoolType;
		Type* CharPointerType;
        Type* VoidType;
        Type* InitializerListType;

		const JetProject* project;

		Namespace* ns;

		std::map<std::string, Source*> sources;
		std::map<std::string, BlockExpression*> asts;
		
		std::vector<InitializerListData*> initializer_lists;

		~Compilation();

		void AdvanceTypeLookup(Type** dest, const std::string& name, Token* location);


		Type* LookupType(const std::string& name, bool load = true, bool error = true, int start_index = 0, bool first_level = true);
		Type* TryLookupType(const std::string& name);

		//Give size of zero for non-array
		CValue AddGlobal(const std::string& name, Type* t, int size, llvm::Constant* init = 0, bool intern = false, bool is_const = false);

		static Compilation* Make(const JetProject* proj, DiagnosticBuilder* builder, bool time = false, int debug = 2, std::string target = "");

		std::vector<Diagnostic>& GetErrors()
		{
			return this->diagnostics->GetErrors();
		}

        void Info(const std::string& string, Token token);
        void Info(const std::string& string, const std::pair<const Token*, const Token*>& tokens);
		void Error(const std::string& string, Token token);
        void Error(const std::string& string, const std::pair<Token, Token>& tokens);
        void Error(const std::string& string, const std::pair<const Token*, const Token*>& tokens);
		void Error(const std::string& string, const Token& start, const Token& end);

		//generates and outputs an exe or lib file
		void Assemble(const std::vector<std::string>& resolved_deps, const std::string& linker = "", int olevel = 0, bool time = false, bool output_ir = false);


		//racer stuff
		Function* GetFunctionAtPoint(const char* file, int line);

		Type* GetArrayType(Type* base);

	private:

		Type* GetFunctionType(Type* return_type, const std::vector<Type*>& args);
		std::map<Type*, Type*> array_types;

		std::map<std::pair<Type*, unsigned int>, Type*> internal_array_types;
		Type* GetInternalArrayType(Type* base, unsigned int size);

		Function* GetFunction(const std::string& name, const std::vector<Type*>& args);

    public:
		Symbol GetVariableOrFunction(const std::string& name);
    private:

		void ResolveTypes();

		void Optimize(int level);

		void OutputIR(const char* filename);
		void OutputPackage(const std::vector<std::string>& resolved_deps, const std::string& project_name, int o_level, bool time = false);

		void Dump()
		{
			if (module)
				module->dump();
		}

		// returns the target used
		std::string SetTarget(const std::string& triple);//make this customizable later
	};

}
#endif
