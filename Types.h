#ifndef JET_TYPES_HEADER
#define JET_TYPES_HEADER

#include <vector>
#include <string>
#include <map>

namespace llvm
{
	class Type;
	class Function;
}
namespace Jet
{
	enum class Types
	{
		Void,
		Double,
		Float,
		Int,
		Char,
		Short,
		Bool,

		Struct,//value type
		Function,

		Pointer,
		Array,//acts just like a pointer

		Invalid,//for unloaded types
	};


	//todo: add void* like type
	class Compiler;
	struct Struct;
	struct Type
	{
		Types type : 8;
		bool loaded : 8;
		union
		{
			Struct* data;//for classes
			Type* base;//for pointers and arrays
		};
		unsigned int size;//for arrays

		Type() { data = 0; type = Types::Void; loaded = false; size = 0; }
		Type(Types type, Struct* data = 0) : type(type), data(data), loaded(false), size(0) {}
		Type(Types type, Type* base, int size = 0) : type(type), base(base), loaded(false), size(size) {}

		void Load(Compiler* compiler);

		Type* Instantiate(Compiler* compiler, const std::vector<Type*>& types);

		std::string ToString();

		//Type* GetPointerType()
		//{
		//todo, idk how im gonna do this lel
		//}
	};

	struct Trait
	{
		
	};

	class StructExpression;
	class Function;
	struct Struct
	{
		std::string name;
		llvm::Type* type;

		std::vector<std::pair<std::string, Type*>> members;//member variables

		std::map<std::string, Function*> functions;//member functions

		bool loaded;

		//template stuff
		Struct* template_base;
		std::vector<std::pair<std::string, std::string>> templates;
		StructExpression* expression;

		Struct()
		{
			template_base = 0;
			type = 0;
			expression = 0;
			loaded = false;
		}

		void Load(Compiler* compiler);
	};
	
	struct Function
	{
		std::string name;

		std::vector<llvm::Type*> args;
		std::vector<std::pair<Type*, std::string>> argst;

		llvm::Function* f;//not always used

		Type* return_type;

		bool loaded;

		Function()
		{
			loaded = false;
		}

		void Load(Compiler* compiler);
	};

	llvm::Type* GetType(Type* t);

	extern Type VoidType;
	extern Type BoolType;
	extern Type DoubleType;
	extern Type IntType;
}
#endif