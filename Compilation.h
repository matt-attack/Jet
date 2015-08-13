#ifndef JET_COMPILATION_HEADER
#define JET_COMPILATION_HEADER

#include <string>
#include <map>
#include <vector>

#include <llvm\IR\IRBuilder.h>
#include <llvm\IR\LLVMContext.h>
#include <llvm\IR\Module.h>

namespace Jet
{
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
	class BlockExpression;
	class Compilation
	{
		friend class Type;
		friend struct Struct;
		friend class Function;
		friend class Compiler;
		friend class Expression;
		friend class CompilerContext;
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

		std::map<std::string, Trait*> traits;

		std::vector<JetError> errors;

		CompilerContext* current_function;

		Namespace* global;

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

		Namespace* ns;

		std::map<std::string, Source*> sources;
		std::map<std::string, BlockExpression*> asts;

		~Compilation();

		std::vector<std::pair<Namespace*, Type**>> types;//a list of all referenced types and their locations
		void AdvanceTypeLookup(Type** dest, const std::string& name);

		//std::map<std::string, Type*> types;
		Type* LookupType(const std::string& name, bool load = true);
		Type* TryLookupType(const std::string& name);

		std::map<std::string, CValue> globals;
		CValue AddGlobal(const std::string& name, Type* t);

		static Compilation* Make(JetProject* proj);

		std::vector<JetError>& GetErrors()
		{
			return this->errors;
		}

		void Error(const std::string& string, Token token);

		//generates and outputs an exe or lib file
		void Assemble(int olevel);

	private:

		Function* GetFunction(const std::string& name, const std::vector<CValue>& args)
		{
			return this->GetFunction(name);
		}

		Function* GetFunction(const std::string& name)
		{
			auto r = this->ns->members.find(name);
			if (r != this->ns->members.end() && r->second.type == SymbolType::Function)
				return r->second.fn;
			//try lower one
			auto next = this->ns->parent;
			while (next)
			{
				r = next->members.find(name);
				if (r != next->members.end() && r->second.type == SymbolType::Function)
					return r->second.fn;

				next = next->parent;
			}
			return 0;
		}

		void ResolveTypes()
		{
			auto oldns = this->ns;
			for (int i = 0; i < this->types.size(); i++)// auto ii : this->types)
			{
				if ((*types[i].second)->type == Types::Invalid)
				{
					//resolve me

					this->ns = types[i].first;

					auto res = this->LookupType((*types[i].second)->name, false);
					*types[i].second = res;
				}
			}
			this->ns = oldns;
		}

		void Optimize(int level);

		void OutputIR(const char* filename);
		void OutputPackage(const std::string& project_name, int o_level);

		void Dump()
		{
			if (module)
				module->dump();
		}

		void SetTarget();//make this customizable later
	};

}
#endif