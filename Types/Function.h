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
	struct Function
	{
		FunctionType* type;
		std::string name;

		std::vector<std::pair<Type*, std::string>> arguments;
		Type* return_type;

		CompilerContext* context;


		llvm::Function* f;//not always used
		llvm::DISubprogram* scope;

		
		//template stuff
		FunctionExpression* template_base;
		std::vector<std::pair<Type*, std::string>> templates;
		FunctionExpression* expression;

		bool loaded;

		Function(const std::string& name)
		{
			this->name = name;
			context = 0;
			f = 0;
			expression = 0;
			loaded = false;
			template_base = 0;
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