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
		llvm::Value* pointer;

		CValue()
		{
			type = &VoidType;
			val = 0;
			pointer = 0;
		}

		CValue(Type* type, llvm::Value* val) : type(type), val(val), pointer(0) {}
		CValue(Type* type, llvm::Value* val, llvm::Value* pointer) : type(type), val(val), pointer(pointer) {}

		llvm::Value* GetReference();
	};

	enum class Types
	{
		Void,//valid really only as a return type
		Double,//64 bit float
		Float,//32 bit float

		//integer types must alternate between signed and unsigned in this group, if not need to fix functions below
		Long,//64 bit type signed
		ULong,//64 bit type unsigned
		Int,//32 bit type signed
		UInt,//32 bit type unsigned
		Char,//8 bit type signed
		UChar,//8 bit type unsigned
		Short,//16 bit type signed
		UShort,//16 bit type unsigned
		Bool,//1 bit integer type

		Struct,//value type
		Union,//tagged unions
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
		void FinishCompilingTemplate(Compilation* compiler);

		std::string ToString();

		//get llvm infos
		llvm::DIType* GetDebugType(Compilation* compiler);
		llvm::Type* GetLLVMType();

		llvm::Constant* GetDefaultValue(Compilation* compilation);

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

		bool IsSignedInteger()
		{
			bool is_int = this->IsInteger();
			bool is_signed = ((int)Types::Long % 2) == ((int)this->type%2);
			return is_int && is_signed;
		}

		bool IsInteger()
		{
			return (this->type >= Types::Long && this->type < Types::Bool);
		}
	};


	enum class SymbolType
	{
		Invalid,
		Namespace,
		Type,
		Function,
		Variable,
	};
	struct Symbol
	{
		SymbolType type : 8;
		union
		{
			Namespace* ns;
			Type* ty;
			Function* fn;
			CValue* val;
		};

		Symbol() { this->type = SymbolType::Invalid; }
		Symbol(Function* fn) : fn(fn) { this->type = SymbolType::Function; }
		Symbol(Type* ty) : ty(ty) { this->type = SymbolType::Type; }
		Symbol(Namespace* ns) : ns(ns) { this->type = SymbolType::Namespace; }
		Symbol(CValue* val) : val(val) { this->type = SymbolType::Variable; }
	};

	struct Namespace
	{
		std::string name;
		Namespace* parent;

		std::multimap<std::string, Symbol> members;

		Namespace() {};

		Namespace(const std::string& name, Namespace* parent)
		{
			this->name = name;
			this->parent = parent;
		}

		virtual ~Namespace();

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
	struct Trait : public Namespace
	{
		bool valid;

		std::multimap<std::string, Function*> functions;
		std::multimap<std::string, Function*> extension_methods;

		//template stuff
		std::vector<std::pair<Type*, std::string>> templates;
		std::vector<Type*> template_args;
	};

	class StructExpression;
	struct Struct : public Namespace
	{
		llvm::Type* type;
		Type* parent_struct;//when inheritance

		bool is_class = false;

		struct StructMember
		{
			std::string name;
			std::string type_name;
			Type* type;
		};
		std::vector<StructMember> struct_members;//member variables
		std::multimap<std::string, Function*> functions;//member functions

		bool loaded;

		//template related stuff
		Struct* template_base;//the uninstantiated function that I was generated from
		std::vector<std::pair<Type*, std::string>> templates;
		std::vector<Type*> template_args;
		StructExpression* expression;

		Struct()
		{
			template_base = 0;
			type = 0;
			expression = 0;
			parent_struct = 0;
			loaded = false;
		}

		bool IsParent(Type* ty);

		void Load(Compilation* compiler);
	};

	struct Union
	{
		std::vector<Type*> members;
		llvm::Type* type;
	};
}
#endif