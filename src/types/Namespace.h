#ifndef JET_TYPES_NAMESPACE_HEADER
#define JET_TYPES_NAMESPACE_HEADER

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
	enum class SymbolType: char
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
        Token token;

		Symbol() { this->type = SymbolType::Invalid; }
		Symbol(Function* fn) : fn(fn) { if (fn) this->type = SymbolType::Function; else this->type = SymbolType::Invalid; }
		Symbol(Type* ty) : ty(ty) { this->type = SymbolType::Type; }
		Symbol(Namespace* ns) : ns(ns) { this->type = SymbolType::Namespace; }
		Symbol(CValue* val) : val(val) { this->type = SymbolType::Variable; }
		Symbol(CValue* val, const Token& t) : val(val), token(t) { this->type = SymbolType::Variable; }

        operator bool() { return type != SymbolType::Invalid; } 
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

		void OutputMetadata(std::string& data, Compilation* comp, bool globals);

		const std::string& GetQualifiedName();

	private:
		std::string qualified_name_;
	};
}
#endif
