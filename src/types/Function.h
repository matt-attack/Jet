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

        std::string ToString()
        {
            std::string str;
            str += return_type ? return_type->ToString() : "void";
            str += '(';
            for (int i = 0; i < args.size(); i++)
            {
                str += args[i]->ToString();
                if (i != args.size() - 1)
                {
                    str += ", ";
                }
            }
            str += ')';
            return str;
        }

        CValue Call(CompilerContext* context, llvm::Value* val, const std::vector<CValue>& args, const bool devirtualize = false, const Function* f = 0, const bool bonus_arg = false);
	};
	
	class FunctionExpression;
    class ExternExpression;

	enum class CallingConvention
	{
		Default,
		StdCall,
		FastCall,
		ThisCall
	};
	struct Function
	{
        friend struct FunctionType;
		FunctionType* type_ = 0;
		CallingConvention calling_convention_;

		// the prefix for the mangled function name
		std::string name_;

		std::vector<std::pair<Type*, std::string>> arguments_;
		Type* return_type_;

		CompilerContext* context_ = 0;

		bool do_export_;
		bool has_return_ = false;

		//generator stuff
		bool is_generator_ = false;
		struct generator_data
		{
			llvm::IndirectBrInst* ibr;
			int var_num;
			std::vector<llvm::Value*> variable_geps;//for generator variables
		} generator_;

		//lambda stuff
		bool is_lambda_;
		struct lambda_data
		{
			llvm::StructType* storage_type;//for lambdas
		} lambda_;

		// Mangling related settings
		bool is_c_function_ = false;
        bool is_const_ = false;
        bool is_extern_;

		//virtual stuff
		bool is_virtual_ = false;
		int virtual_offset_ = -1;
		int virtual_table_location_ = -1;

		llvm::Function* f_ = 0;//not always used
		llvm::DISubprogram* scope_;

		
		//template stuff
		FunctionExpression* template_base_ = 0;
		std::vector<std::pair<Type*, std::string>> templates_;
		FunctionExpression* expression_ = 0;
        ExternExpression* extern_expression_ = 0;//todo make this not different

		bool loaded_ = false;

		Function(const std::string& name, bool is_lambda, bool is_c_function = false, bool is_extern = false)
		{
			calling_convention_ = CallingConvention::Default;
			do_export_ = !is_lambda;
			name_ = name;
			generator_.var_num = 0;
			is_c_function_ = is_c_function;
			is_lambda_ = is_lambda;
            is_extern_ = is_extern;
		}

		~Function();

		CValue Call(CompilerContext* context, const std::vector<CValue>& argsv,
          const bool devirtualize, const bool bonus_arg = false);

		bool IsCompatible(Function* f)
		{
			if (f->return_type_ != return_type_)
				return false;

			if (f->arguments_.size() != arguments_.size())
				return false;

			for (unsigned int i = 0; i < f->arguments_.size(); i++)
				if (f->arguments_[i].first != arguments_[i].first)
					return false;

			return true;
		}

		void Load(Compilation* compiler);

		Type* GetType(Compilation* compiler) const;

		Function* Instantiate(Compilation* compiler, const std::vector<Type*>& types);
	};
}
#endif
