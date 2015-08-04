#ifndef JET_TYPES_HEADER
#define JET_TYPES_HEADER

#include <vector>
#include <string>
#include <map>

#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/DIBuilder.h>

namespace llvm
{
	class Type;
	class Function;
}
namespace Jet
{
	std::string ParseType(const char* tname, int& p);

	class Type;
	extern Type VoidType;
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

	enum class Types
	{
		Void,
		Double,
		Float,
		Long,
		Int,
		Char,
		Short,
		Bool,

		Struct,//value type
		Function,

		Trait,//cant instantiate this!!

		Pointer,
		Array,//acts just like a pointer

		Invalid,//for unloaded types
	};


	//todo: add void* like type
	class Compiler;
	class Compilation;
	class CompilerContext;
	struct Struct;
	struct Trait;
	struct FunctionType;
	struct Function;
	class Type
	{
		std::vector<std::pair<Type**, Trait*>> traits;//all the traits that apply to this type

	public:

		Types type : 8;
		bool loaded : 8;
		union
		{
			Struct* data;//for classes
			Type* base;//for pointers and arrays
			Trait* trait;
			FunctionType* function;
		};
		unsigned int size;//for arrays

		std::string name;


		std::vector<std::pair<Type**, Trait*>> GetTraits(Compilation* compiler);
		bool MatchesTrait(Compilation* compiler, Trait* trait);

		Type() { data = 0; type = Types::Void; loaded = false; size = 0; }
		Type(std::string name, Types type, Struct *data = 0) : type(type), data(data), loaded(false), size(0), name(name) {}
		Type(std::string name, Types type, Type* base, int size = 0) : type(type), base(base), loaded(false), size(size), name(name) {}

		void Load(Compilation* compiler);

		Type* Instantiate(Compilation* compiler, const std::vector<Type*>& types);

		std::string ToString();

		llvm::DIType GetDebugType(Compilation* compiler);

		Function* GetMethod(const std::string& name, const std::vector<CValue>& args, CompilerContext* context, bool def = false);

		Type* GetPointerType(CompilerContext* context);

		Type* GetBaseType()//returns bottom level type
		{
			if (this->type == Types::Pointer)
			{
				this->base->GetBaseType();
			}
			else if (this->type == Types::Array)
			{
				return this->base->GetBaseType();
			}
			else
			{
				return this;
			}
		}
	};

	class Function;
	struct Trait
	{
		bool valid;
		std::string name;

		std::multimap<std::string, Function*> funcs;
		std::multimap<std::string, Function*> extension_methods;

		//template stuff
		std::vector<std::pair<Type*,std::string>> templates;
		std::vector<Type*> template_args;
	};

	class StructExpression;
	struct Struct
	{
		std::string name;
		llvm::Type* type;
		Type* parent;//when inheritance

		struct StructMember
		{
			std::string name;
			std::string type_name;
			Type* type;
		};
		std::vector<StructMember> members;//member variables

		std::multimap<std::string, Function*> functions;//member functions

		bool loaded;

		//template stuff
		Struct* template_base;
		std::vector<std::pair<Type*, std::string>> templates;
		std::vector<Type*> template_args;
		StructExpression* expression;

		//std::vector<std::pair<std::vector<Type*>, Type*>> instantiations;

		Struct()
		{
			template_base = 0;
			type = 0;
			expression = 0;
			parent = 0;
			loaded = false;
		}

		void Load(Compilation* compiler);
	};

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

		llvm::DISubprogram scope;

		
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

	llvm::Type* GetType(Type* t);
}
#endif