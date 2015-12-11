#ifndef JET_FUNCTION_HEADER
#define JET_FUNCTION_HEADER

#include <vector>
#include <string>
#include <map>

namespace llvm
{
	class Type;
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
		StdCall
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
		
		bool is_generator;//generator stuff
		llvm::IndirectBrInst* ibr;

		//lambda stuff
		bool is_lambda;
		llvm::StructType* storage_type;//for lambdas

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
			context = 0;
			f = 0;
			this->is_lambda = is_lambda;
			expression = 0;
			loaded = false;
			template_base = 0;
			this->is_generator = false;
		}

		bool IsCompatible(Function* f)
		{
			if (f->return_type != this->return_type)
				return false;

			if (f->arguments.size() != this->arguments.size())
				return false;

			for (int i = 0; i < f->arguments.size(); i++)
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