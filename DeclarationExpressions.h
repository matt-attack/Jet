#ifndef _DECLARATION_EXPRESSIONS_HEADER
#define _DECLARATION_EXPRESSIONS_HEADER

#include <string>
#include <stdio.h>
#include <vector>

#include "Compiler.h"
#include "CompilerContext.h"
#include "ExpressionVisitor.h"
#include "Source.h"
#include "Types\Function.h"
#include "Expressions.h"

namespace Jet
{
	class Source;
	class Compiler;

	class LocalExpression : public Expression
	{
		Token token, equals;
		std::vector<std::pair<Token, Token>>* _names;
		std::vector<std::pair<Token, Expression*>>* _right;
	public:
		LocalExpression(Token token, Token equals, std::vector<std::pair<Token, Token>>* names, std::vector<std::pair<Token, Expression*>>* right)
		{
			this->equals = equals;
			this->token = token;
			this->_names = names;
			this->_right = right;
		}

		~LocalExpression()
		{
			if (this->_right)
				for (auto ii : *this->_right)
					delete ii.second;

			delete this->_right;
			delete this->_names;
		}

		virtual void SetParent(Expression* parent)
		{
			this->parent = parent;
			if (this->_right)
				for (auto ii : *_right)
					ii.second->SetParent(this);
		}

		CValue Compile(CompilerContext* context);

		void CompileDeclarations(CompilerContext* context) {};

		virtual Type* TypeCheck(CompilerContext* context)
		{
			//register the local
			int i = 0;
			for (auto ii : *this->_names)
			{
				if (ii.first.text.length())
					context->TCRegisterLocal(ii.second.text, context->root->LookupType(ii.first.text, false)->GetPointerType());
				else
				{
					//type inference
					auto ty = (*this->_right)[i].second->TypeCheck(context);
					context->TCRegisterLocal(ii.second.text, ty->GetPointerType());
				}
				i++;
			}

			return 0;
		}


		void Print(std::string& output, Source* source)
		{
			this->token.Print(output, source);

			for (auto ii : *_names)
			{
				if (ii.first.text.length() > 0)
					ii.first.Print(output, source);
				ii.second.Print(output, source);
			}

			if (this->_right)
				this->equals.Print(output, source);

			int i = 0;
			for (auto ii : *_names)
			{
				if (_right && i < _right->size())
				{
					if ((*_right)[i].first.text.length())
						(*_right)[i].first.Print(output, source);// output += " ="; fixme
					(*_right)[i++].second->Print(output, source);
				}
		}
	}

	virtual void Visit(ExpressionVisitor* visitor)
	{
		visitor->Visit(this);

		if (this->_right)
		{
			for (auto ii : *this->_right)
			{
				ii.second->Visit(visitor);
			}
		}
	}
};

class UnionExpression : public Expression
{
	Token token, name, equals;
	std::vector<std::pair<Token, Token>> elements;
public:

	UnionExpression(Token token, Token& name, Token& equals, std::vector<std::pair<Token, Token>>&& elements)
	{
		this->token = token;
		this->elements = elements;
		this->name = name;
		this->equals = equals;
	}

	CValue Compile(CompilerContext* context)
	{
		return CValue();
	}

	void CompileDeclarations(CompilerContext* context)
	{
		//define it
		Type* ty = new Type(this->name.text, Types::Union);
		ty->_union = new Union;
		ty->_union->members.resize(this->elements.size());
		for (int i = 0; i < this->elements.size(); i++)
			context->root->AdvanceTypeLookup(&ty->_union->members[i], this->elements[i].first.text, &this->elements[i].first);

		context->root->ns->members.insert({ this->name.text, ty });
	};

	void Print(std::string& output, Source* source)
	{
		token.Print(output, source);
		name.Print(output, source);
		equals.Print(output, source);

		for (auto ii : this->elements)
		{
			ii.first.Print(output, source);
			if (ii.second.text.length())
				ii.second.Print(output, source);//print the |
		}
	}

	virtual Type* TypeCheck(CompilerContext* context)
	{
		//get the type and finish it
		//context->root->LookupType(this->name.text, false);

		//lookup all the members and add them

		//get the type fixme later
		return 0;
	}

	virtual void Visit(ExpressionVisitor* visitor)
	{
		visitor->Visit(this);
	}
};

class NamespaceExpression : public Expression
{
	Token name;
	Token token;
	BlockExpression* block;
public:
	NamespaceExpression(Token token, Token name, BlockExpression* block)
	{
		this->name = name;
		this->token = token;
		this->block = block;
	}

	void SetParent(Expression* parent)
	{
		this->parent = parent;
		this->block->SetParent(this);
	}

	CValue Compile(CompilerContext* context)
	{
		context->SetNamespace(this->name.text);

		this->block->Compile(context);

		context->PopNamespace();

		return CValue();
	}

	virtual Type* TypeCheck(CompilerContext* context)
	{
		//push namespace
		context->SetNamespace(this->name.text);

		this->block->TypeCheck(context);

		context->PopNamespace();

		return 0;
	}

	void CompileDeclarations(CompilerContext* context)
	{
		context->SetNamespace(this->name.text);

		this->block->CompileDeclarations(context);

		context->PopNamespace();
	}

	void Print(std::string& output, Source* source)
	{
		token.Print(output, source);
		name.Print(output, source);
		block->Print(output, source);
	}

	virtual void Visit(ExpressionVisitor* visitor)
	{
		visitor->Visit(this);
	}
};

struct FunctionArg
{
	Token type, name, comma;
};
class FunctionExpression : public Expression
{
	friend class StructExpression;
	friend class Namespace;
	friend class Compiler;
	friend class CompilerContext;
	friend class Type;
	friend class Function;
	friend struct Struct;
	Token name;
	std::vector<FunctionArg>* args;
	std::vector<Token>* captures;
	ScopeExpression* block;
	Token token;

	Token Struct, colons;

	Token ret_type;
	NameExpression* varargs;

	bool is_generator;
	std::vector<std::pair<Token, Token>>* templates;
	Token open_bracket, close_bracket;
public:

	ScopeExpression* GetBlock()
	{
		return block;
	}

	FunctionExpression(Token token, Token name, Token ret_type, bool generator, std::vector<FunctionArg>* args, ScopeExpression* block, /*NameExpression* varargs = 0,*/ Token Struct, Token colons, std::vector<std::pair<Token, Token>>* templates, std::vector<Token>* captures, Token open_bracket, Token close_bracket)
	{
		this->open_bracket = open_bracket;
		this->close_bracket = close_bracket;
		this->is_generator = generator;
		this->ret_type = ret_type;
		this->args = args;
		this->block = block;
		this->name = name;
		this->token = token;
		this->varargs = 0;// varargs;
		this->Struct = Struct;
		this->templates = templates;
		this->captures = captures;
		this->colons = colons;
	}

	~FunctionExpression()
	{
		delete block;
		delete varargs;
		delete args;
		delete captures;
	}

	std::string GetRealName();

	std::string GetName()
	{
		return this->name.text;
	}

	void SetParent(Expression* parent)
	{
		this->parent = parent;
		block->SetParent(this);
	}

	virtual Type* TypeCheck(CompilerContext* context);

	CValue Compile(CompilerContext* context);
	void CompileDeclarations(CompilerContext* context);

	void Print(std::string& output, Source* source)
	{
		if (this->name.text.length() == 0)
			throw 7;//todo: implement lamdbas
		//add tokens for the ( )
		token.Print(output, source);

		ret_type.Print(output, source);

		if (this->Struct.text.length())
		{
			this->Struct.Print(output, source);
			//add the ::
			this->colons.Print(output, source);
		}

		name.Print(output, source);

		open_bracket.Print(output, source);

		for (auto ii : *this->args)
		{
			ii.type.Print(output, source);
			ii.name.Print(output, source);
			if (ii.comma.text.length())
				ii.comma.Print(output, source);
		}

		close_bracket.Print(output, source);

		this->block->Print(output, source);
	}

	virtual void Visit(ExpressionVisitor* visitor)
	{
		visitor->Visit(this);
		block->Visit(visitor);
	}

	CValue FunctionExpression::DoCompile(CompilerContext* context);//call this to do real compilation
};

struct ExternArg
{
	Token type, name, comma;
};
class ExternExpression : public Expression
{
	Token name, fun;
	std::string Struct;
	std::vector<ExternArg>* args;
	Token token;
	Token ret_type;
	Token open_bracket, close_bracket;
public:

	ExternExpression(Token token, Token fun, Token name, Token ret_type, Token ob, std::vector<ExternArg>* args, Token cb, std::string str = "")
	{
		this->open_bracket = ob;
		this->close_bracket = cb;
		this->fun = fun;
		this->args = args;
		this->name = name;
		this->token = token;
		this->ret_type = ret_type;
		this->Struct = str;
	}

	~ExternExpression()
	{
		delete args;
	}

	void SetParent(Expression* parent)
	{
		this->parent = parent;
	}

	CValue Compile(CompilerContext* context);

	void CompileDeclarations(CompilerContext* context);

	virtual Type* TypeCheck(CompilerContext* context)
	{
		return 0;
	}

	void Print(std::string& output, Source* source)
	{
		//add tokens for the ( )
		token.Print(output, source);

		fun.Print(output, source);// output += " fun"; fixme
		ret_type.Print(output, source);

		name.Print(output, source);

		open_bracket.Print(output, source);// output += "(";
		int i = 0;
		for (auto ii : *this->args)
		{
			ii.type.Print(output, source);
			ii.name.Print(output, source);
			if (ii.comma.text.length())
				ii.comma.Print(output, source);
		}
		close_bracket.Print(output, source);
		//output += ")";
	}

	virtual void Visit(ExpressionVisitor* visitor)
	{
		visitor->Visit(this);
	}
};

struct TraitFunctionArg
{
	Token type, name, comma;
};
struct TraitFunction
{
	Token name, func_token, open_brace, close_brace;
	Token ret_type, semicolon;

	std::vector<TraitFunctionArg> args;
};
struct TraitTemplate
{
	Token name, comma;
};
class TraitExpression : public Expression
{
	Token token;
	Token name;
	Token open_bracket, close_bracket;
	Token topen_bracket, tclose_bracket;
	std::vector<TraitFunction> funcs;
	std::vector<TraitTemplate>* templates;
public:

	TraitExpression(Token token, Token name, Token tob, Token tcb, Token ob, std::vector<TraitFunction>&& funcs, std::vector<TraitTemplate>* templates, Token cb)
	{
		this->topen_bracket = tob;
		this->tclose_bracket = tcb;
		this->token = token;
		this->name = name;
		this->funcs = funcs;
		this->templates = templates;
		this->open_bracket = ob;
		this->close_bracket = cb;
	}

	CValue Compile(CompilerContext* context)
	{
		return CValue();
	}

	void CompileDeclarations(CompilerContext* context);

	void Print(std::string& output, Source* source)
	{
		token.Print(output, source);
		name.Print(output, source);

		if (this->templates)//print templates
		{
			this->topen_bracket.Print(output, source);
			for (auto ii : *this->templates)
			{
				ii.name.Print(output, source);
				if (ii.comma.text.length())
					ii.comma.Print(output, source);
			}
			this->tclose_bracket.Print(output, source);
		}

		open_bracket.Print(output, source);// output += "{"; fixme

		for (auto fun : this->funcs)
		{
			fun.func_token.Print(output, source);// output += " fun ";
			fun.ret_type.Print(output, source);
			fun.name.Print(output, source);
			fun.open_brace.Print(output, source);// output += "(";
			//bool first = false;
			for (auto arg : fun.args)
			{
				/*if (first)
					output += ", ";fix me
					else
					first = true;*/

				arg.type.Print(output, source);

				arg.name.Print(output, source);

				if (arg.comma.text.length())
					arg.comma.Print(output, source);
				//output += arg.first->ToString() + " " + arg.second;
			}
			fun.close_brace.Print(output, source);// output += ");";
			fun.semicolon.Print(output, source);
		}
		close_bracket.Print(output, source);// output += "}";
	}

	virtual Type* TypeCheck(CompilerContext* context)
	{
		return 0;
	}

	virtual void Visit(ExpressionVisitor* visitor)
	{
		visitor->Visit(this);
	}
};

struct StructVariable
{
	Token type, name, semicolon;
};
struct StructMember
{
	enum MemberType
	{
		FunctionMember,
		VariableMember,
	};
	MemberType type;

	FunctionExpression* function;
	StructVariable variable;
};
struct StructTemplate
{
	Token type, name, comma;
};
class StructExpression : public Expression
{
	friend class Compiler;
	friend class Type;

	Token token;
	Token name;


	Token start;
	Token end;

	Token colon;
	Token base_type;

	void AddConstructorDeclarations(Type* str, CompilerContext* context);
	void AddConstructors(CompilerContext* context);
public:

	std::vector<StructTemplate>* templates;

	std::vector<StructMember> members;

	Token template_open, template_close;

	StructExpression(Token token, Token name, Token start, Token end, Token ob, std::vector<StructMember>&& members, std::vector<StructTemplate>* templates, Token cb, Token colon, Token base_type)
	{
		this->templates = templates;
		this->members = members;
		this->base_type = base_type;
		this->name = name;
		this->token = token;
		this->start = start;
		this->template_open = ob;
		this->template_close = cb;
		this->end = end;
		this->colon = colon;
	}

	~StructExpression()
	{
		for (auto ii : members)
			if (ii.type == StructMember::FunctionMember)
				delete ii.function;
	}

	std::string GetName()
	{
		return this->name.text;
	}

	virtual Type* TypeCheck(CompilerContext* context);

	void SetParent(Expression* parent)
	{
		this->parent = parent;
		for (auto ii : this->members)
		{
			if (ii.type == StructMember::FunctionMember)
				ii.function->SetParent(this);
		}
	}

	CValue Compile(CompilerContext* context);

	void CompileDeclarations(CompilerContext* context);

	void Print(std::string& output, Source* source)
	{
		//add tokens for the ( )
		token.Print(output, source);

		name.Print(output, source);
		//output templates
		if (this->templates && this->templates->size() > 0)
		{
			template_open.Print(output, source);// output += "<"; fixme

			for (auto ii : *this->templates)
			{
				ii.type.Print(output, source);
				ii.name.Print(output, source);
				if (ii.comma.text.length())
					ii.comma.Print(output, source);
			}
			template_close.Print(output, source);// output += ">";
		}

		if (this->colon.text.length())
		{
			this->colon.Print(output, source);
			this->base_type.Print(output, source);
		}

		this->start.Print(output, source);

		for (auto ii : this->members)
		{
			if (ii.type == StructMember::FunctionMember)
				ii.function->Print(output, source);
			else
			{
				ii.variable.type.Print(output, source);
				ii.variable.name.Print(output, source);
				ii.variable.semicolon.Print(output, source);// output += ";"; fixme
			}
		}

		this->end.Print(output, source);
	}

	virtual void Visit(ExpressionVisitor* visitor)
	{
		visitor->Visit(this);

		for (auto ii : members)
		{
			if (ii.type == StructMember::FunctionMember)
				ii.function->Visit(visitor);
		}
	}
};

class AttributeExpression : public Expression
{
	Token open_bracket, close_bracket;
	Expression* next;
public:

	Token name;
	AttributeExpression(Token token, Token name, Token cb, Expression* next) : open_bracket(token), name(name), next(next), close_bracket(cb)
	{

	}

	~AttributeExpression()
	{
		delete next;
	}

	void SetParent(Expression* parent)
	{
		this->parent = parent;
		this->next->parent = this;
	}

	void Print(std::string& output, Source* source)
	{
		open_bracket.Print(output, source);
		name.Print(output, source);
		close_bracket.Print(output, source);
		next->Print(output, source);
	}

	void CompileDeclarations(CompilerContext* context)
	{
		next->CompileDeclarations(context);
	}

	virtual Type* TypeCheck(CompilerContext* context)
	{
		//just need to make sure the attribute exists
		//throw 7;
		next->TypeCheck(context);
		return 0;
	}

	virtual void Visit(ExpressionVisitor* visitor)
	{
		visitor->Visit(this);
		next->Visit(visitor);
	}

	CValue Compile(CompilerContext* context)
	{
		return next->Compile(context);
	}
};

class TypedefExpression : public Expression
{
	Token token;
	Token new_type;
	Token equals;
	Token other_type;
public:
	TypedefExpression(Token token, Token new_type, Token equals, Token other_type) : token(token), new_type(new_type),
		equals(equals), other_type(other_type) {}

	void SetParent(Expression* parent)
	{
		this->parent = parent;
	}

	CValue Compile(CompilerContext* context)
	{
		if (this->parent->parent != 0)
			context->root->Error("Cannot use typedef outside of global scope", token);

		return CValue();
	}

	virtual Type* TypeCheck(CompilerContext* context)
	{
		return 0;
	}

	void CompileDeclarations(CompilerContext* context)
	{
		if (this->parent->parent != 0)
			context->root->Error("Cannot use typedef outside of global scope", token);

		context->root->ns->members.insert({ this->new_type.text, context->root->LookupType(this->other_type.text) });
		//context->root->Error("Typedef not implemented atm", token);

		/*context->CurrentToken(&other_type);
		auto type = context->root->AdvanceTypeLookup(other_type.text);

		//this can be fixed by adding a third pass over the typedefs
		auto iter = context->root->types.find(new_type.text);
		if (iter != context->root->types.end())
		{
		if (iter->second->type == Types::Invalid)
		context->root->Error("Please place the typedef before it is used, after is not yet handled properly.", new_type);
		else
		context->root->Error("Type '" + new_type.text + "' already defined!", new_type);
		}
		context->root->types[new_type.text] = type;*/
	};

	void Print(std::string& output, Source* source)
	{
		token.Print(output, source);
		new_type.Print(output, source);
		equals.Print(output, source);
		other_type.Print(output, source);
	}

	virtual void Visit(ExpressionVisitor* visitor)
	{
		visitor->Visit(this);
	}
};
}
#endif