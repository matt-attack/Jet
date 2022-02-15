#ifndef JET_TYPES_STRUCT_HEADER
#define JET_TYPES_STRUCT_HEADER

#include <vector>
#include <string>
#include <map>

#include "../Token.h"
#include "Namespace.h"

#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/DIBuilder.h>

namespace llvm
{
	class Type;
	class Function;
}
namespace Jet
{
	struct Function;
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
		std::map<std::string, Function*> functions;//member functions
        std::vector<Function*> constructors;//constructors

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
