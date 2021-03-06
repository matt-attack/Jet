#ifndef JET_FUNCTION_HEADER
#define JET_FUNCTION_HEADER

#include <vector>
#include <string>
#include <map>

#include "Types.h"

namespace llvm
{
	class Type;
	class Value;
	class Function;
	class DISubprogram;
	class StructType;
	class IndirectBrInst;
}
namespace Jet
{
	class Type;
	class Compiler;
	class Compilation;
	class CompilerContext;
	struct FunctionType
	{
		bool loaded;
		Type* return_type;
		std::vector<Type*> args;

		//llvm::FunctionType* type;

		FunctionType()
		{
			loaded = false;
		}

		void Load(Compiler* compiler)
		{

		}
	};
	
	class FunctionExpression;

	enum class CallingConvention
	{
		Default,
		StdCall,
		FastCall,
		ThisCall
	};
	struct Function
	{
		FunctionType* type;
		CallingConvention calling_convention;

		std::string name;

		std::vector<std::pair<Type*, std::string>> arguments;
		Type* return_type;

		CompilerContext* context;

		bool do_export;
		bool has_return = false;

		//generator stuff
		bool is_generator;
		struct generator_data
		{
			llvm::IndirectBrInst* ibr;
			int var_num;
			std::vector<llvm::Value*> variable_geps;//for generator variables
		} generator;

		//lambda stuff
		bool is_lambda;
		struct lambda_data
		{
			llvm::StructType* storage_type;//for lambdas
		} lambda;

		//virtual stuff
		bool is_virtual = false;
		int virtual_offset = -1;
		int virtual_table_location = -1;

		llvm::Function* f;//not always used
		llvm::DISubprogram* scope;

		
		//template stuff
		FunctionExpression* template_base;
		std::vector<std::pair<Type*, std::string>> templates;
		FunctionExpression* expression;

		bool loaded;

		Function(const std::string& name, bool is_lambda)
		{
			this->calling_convention = CallingConvention::Default;
			this->do_export = true;
			this->name = name;
			this->generator.var_num = 0;
			context = 0;
			f = 0;
			this->is_lambda = is_lambda;
			expression = 0;
			loaded = false;
			template_base = 0;
			this->is_generator = false;
		}

		~Function()
		{
			delete this->context;
		}

		CValue Call(CompilerContext* context, const std::vector<CValue>& argsv, bool devirtualize);

		bool IsCompatible(Function* f)
		{
			if (f->return_type != this->return_type)
				return false;

			if (f->arguments.size() != this->arguments.size())
				return false;

			for (unsigned int i = 0; i < f->arguments.size(); i++)
				if (f->arguments[i].first != this->arguments[i].first)
					return false;

			return true;
		}

		void Load(Compilation* compiler);

		Type* GetType(Compilation* compiler);

		Function* Instantiate(Compilation* compiler, const std::vector<Type*>& types);
	};
}
#endif