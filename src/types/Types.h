#ifndef JET_TYPES_HEADER
#define JET_TYPES_HEADER

#include <vector>
#include <string>
#include <map>

#include "../Token.h"

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
	struct CValue
	{
		//my type info
		Type* type;
		llvm::Value* val;
		llvm::Value* pointer;
        bool is_const;

		CValue(Type* type, llvm::Value* val) 
          : type(type), val(val), pointer(0), is_const(false) {}
		CValue(Type* type, llvm::Value* val, llvm::Value* pointer, bool _is_const = false)
          : type(type), val(val), pointer(pointer), is_const(_is_const) {}
	};

	enum class Types: char
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
		Array,//acts just like a pointer but really is a struct with size contained
		InternalArray,//used for things like global arrays


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
		llvm::Type* llvm_type = 0;

        Compilation* compilation;
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

		Type(Compilation* comp, std::string name, Types type, Struct *data = 0) 
          : compilation(comp), type(type), data(data), loaded(false), size(0),
            name(name), pointer_type(0), debug_type(0) {}
		Type(Compilation* comp, std::string name, Types type, Type* base, int size = 0)
          : compilation(comp), type(type), base(base), loaded(false), size(size),
            name(name), pointer_type(0), debug_type(0) {}

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
				return this->base->GetBaseType();
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
}
#endif
