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
		Union,
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
	struct Namespace;
	struct Union;
	struct Token;
	class Type
	{
		friend class Compilation;
		std::vector<std::pair<Type**, Trait*>> traits;//all the traits that apply to this type

		//cached stuff;
		llvm::DIType* debug_type;
		Type* pointer_type;
	public:

		Types type : 8;
		bool loaded : 8;
		union
		{
			Struct* data;//for classes
			Type* base;//for pointers and arrays
			Trait* trait;
			FunctionType* function;
			Token* _location;
			Union* _union;
		};
		unsigned int size;//for arrays

		std::string name;
		Namespace* ns;

		std::vector<std::pair<Type**, Trait*>> GetTraits(Compilation* compiler);
		bool MatchesTrait(Compilation* compiler, Trait* trait);

		Type() : debug_type(0) { data = 0; type = Types::Void; loaded = false; size = 0; pointer_type = 0; }
		Type(std::string name, Types type, Struct *data = 0) : type(type), data(data), loaded(false), size(0), name(name), pointer_type(0), debug_type(0) {}
		Type(std::string name, Types type, Type* base, int size = 0) : type(type), base(base), loaded(false), size(size), name(name), pointer_type(0), debug_type(0) {}

		void Load(Compilation* compiler);

		Type* Instantiate(Compilation* compiler, const std::vector<Type*>& types);

		std::string ToString();

		//get llvm infos
		llvm::DIType* GetDebugType(Compilation* compiler);
		llvm::Type* GetLLVMType();


		Function* GetMethod(const std::string& name, const std::vector<Type*>& args, CompilerContext* context, bool def = false);

		Type* GetPointerType();

		Type* GetBaseType()//returns bottom level type
		{
			if (this->type == Types::Pointer)
				this->base->GetBaseType();
			else if (this->type == Types::Array)
				return this->base->GetBaseType();
			else
				return this;
		}

		int GetSize();

		//can it be instantiated? (no Traits as subtypes)
		bool IsValid();
	};

	struct Namespace;
	struct Function;
	enum class SymbolType
	{
		Namespace,
		Type,
		Function
	};
	struct Symbol
	{
		SymbolType type : 8;
		union
		{
			Namespace* ns;
			Type* ty;
			Function* fn;
		};

		Symbol() {}
		Symbol(Function* fn) : fn(fn) { this->type = SymbolType::Function; }
		Symbol(Type* ty) : ty(ty) { this->type = SymbolType::Type; }
		Symbol(Namespace* ns) : ns(ns) { this->type = SymbolType::Namespace; }
	};

	struct Namespace
	{
		std::string name;
		Namespace* parent;

		std::multimap<std::string, Symbol> members;

		~Namespace();

		Function* GetFunction(const std::string& name)
		{
			auto r = members.find(name);
			if (r != members.end() && r->second.type == SymbolType::Function)
				return r->second.fn;
			return 0;
		}

		Function* GetFunction(const std::string& name, const std::vector<CValue>& args)
		{
			auto r = members.find(name);
			if (r != members.end() && r->second.type == SymbolType::Function)
				return r->second.fn;
			return 0;
		}

		void OutputMetadata(std::string& data, Compilation* comp);
	};

	class Function;
	struct Trait: public Namespace
	{
		bool valid;
		//std::string name;

		std::multimap<std::string, Function*> functions;
		std::multimap<std::string, Function*> extension_methods;

		//template stuff
		std::vector<std::pair<Type*,std::string>> templates;
		std::vector<Type*> template_args;
	};

	class StructExpression;
	struct Struct : public Namespace
	{
		//std::string name;
		llvm::Type* type;
		Type* parent_struct;//when inheritance

		struct StructMember
		{
			std::string name;
			std::string type_name;
			Type* type;
		};
		std::vector<StructMember> struct_members;//member variables

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
			parent_struct = 0;
			loaded = false;
		}

		void Load(Compilation* compiler);
	};

	struct Union
	{
		std::vector<Type*> members;
		llvm::Type* type;
	};
}
#endif