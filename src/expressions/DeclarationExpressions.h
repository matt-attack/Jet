#ifndef _DECLARATION_EXPRESSIONS_HEADER
#define _DECLARATION_EXPRESSIONS_HEADER

#include <string>
#include <stdio.h>
#include <vector>

#include "Compiler.h"
#include "CompilerContext.h"
#include "ExpressionVisitor.h"
#include "Source.h"
#include "types/Function.h"
#include "types/Struct.h"
#include "Expressions.h"

#include "LetExpression.h"
#include "StructExpression.h"
#include "FunctionExpression.h"

namespace Jet
{
	class Source;
	class Compiler;

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
			return CValue(context->root->VoidType, 0);
		}

		void CompileDeclarations(CompilerContext* context)
		{
			//define it
			Type* ty = new Type(context->root, this->name.text, Types::Union);
			ty->_union = new Union;
			ty->_union->members.resize(this->elements.size());
			for (unsigned int i = 0; i < this->elements.size(); i++)
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
		Token token;
		BlockExpression* block;
		std::vector<std::pair<Token, Token>>* names;
	public:
		NamespaceExpression(Token token, std::vector<std::pair<Token, Token>>* names, BlockExpression* block)
		{
			this->token = token;
			this->block = block;
			this->names = names;
		}

		~NamespaceExpression()
		{
			delete this->names;
		}

		void SetParent(Expression* parent)
		{
			this->parent = parent;
			this->block->SetParent(this);
		}

		CValue Compile(CompilerContext* context)
		{
			for (unsigned int i = 0; i < this->names->size(); i++)
				context->SetNamespace((*this->names)[i].first.text);

			this->block->Compile(context);

			for (unsigned int i = 0; i < this->names->size(); i++)
				context->PopNamespace();

			return CValue(context->root->VoidType, 0);
		}

		virtual Type* TypeCheck(CompilerContext* context)
		{
			//push namespace
			for (unsigned int i = 0; i < this->names->size(); i++)
				context->SetNamespace((*this->names)[i].first.text);

			this->block->TypeCheck(context);

			for (unsigned int i = 0; i < this->names->size(); i++)
				context->PopNamespace();

			return 0;
		}

		void CompileDeclarations(CompilerContext* context)
		{
			for (unsigned int i = 0; i < this->names->size(); i++)
				context->SetNamespace((*this->names)[i].first.text);

			this->block->CompileDeclarations(context);

			for (unsigned int i = 0; i < this->names->size(); i++)
				context->PopNamespace();
		}

		void Print(std::string& output, Source* source)
		{
			token.Print(output, source);
			for (unsigned int i = 0; i < this->names->size(); i++)
			{
				(*this->names)[i].first.Print(output, source);

				if ((*this->names)[i].second.text.length())
					(*this->names)[i].second.Print(output, source);
			}
			block->Print(output, source);
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);
		}

		// returns the namespace that we create, if any
		std::string namespaceprefix_;
		virtual const char* GetNamespace()
		{
			if (namespaceprefix_.length())
			{
				return namespaceprefix_.c_str();
			}

			for (int i = 0; i < names->size(); i++)
			{
				namespaceprefix_ += (*names)[i].first.text;
				if (i != names->size() - 1)
				{
					namespaceprefix_ += "__";
				}
			}
			return namespaceprefix_.c_str();
		}
	};

	struct ExternArg
	{
		Token type, name, comma;
	};
	class ExternExpression : public Expression
	{
        friend class Function;
		Token name, type;
		std::string Struct;
		std::vector<ExternArg>* args;
		Token token;
		Token ret_type;
		Token open_bracket, close_bracket;
	public:

		ExternExpression(Token token, Token type, Token name, std::string ns = "")
		{
			this->type = type;
			this->name = name;
			this->token = token;
			this->Struct = ns;// todo rename this
            this->args = 0;
		}

		ExternExpression(Token token, Token type, Token name, Token ret_type, Token ob, std::vector<ExternArg>* args, Token cb, std::string str = "")
		{
			this->open_bracket = ob;
			this->close_bracket = cb;
			this->type = type;
			this->args = args;
			this->name = name;
			this->token = token;
			this->ret_type = ret_type;
			this->Struct = str;
		}

		~ExternExpression()
		{
            if (args)
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
			token.Print(output, source);

            if (type.type == TokenType::Name)
            {
                type.Print(output, source);

                name.Print(output, source);

                return;
            }

			type.Print(output, source);// output += " fun"; fixme
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
			return CValue(context->root->VoidType, 0);
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

	struct EnumValue
	{
		Token name, equals, value;
		Token comma;
	};

	class EnumExpression : public Expression
	{
		Token token, name, open, close;
		std::vector<EnumValue> values;
	public:
		EnumExpression(Token token, Token name, Token open, Token close, std::vector<EnumValue>&& val)
		{
			this->token = token;
			this->name = name;
			this->close = close;
			this->open = open;
			this->values = val;
		}

		CValue Compile(CompilerContext* context)
		{
			return CValue(context->root->VoidType, 0);
		}

		void SetParent(Expression* parent)
		{
			this->parent = parent;
		}

		virtual Type* TypeCheck(CompilerContext* context)
		{
			//todo
			return 0;
		}

		void CompileDeclarations(CompilerContext* context);

		void Print(std::string& output, Source* source)
		{
			this->token.Print(output, source);
			this->name.Print(output, source);

			this->open.Print(output, source);

			for (auto ii : this->values)
			{
				ii.name.Print(output, source);

				if (ii.equals.text.length())
				{
					ii.equals.Print(output, source);
					ii.value.Print(output, source);
				}
				if (ii.comma.text.length())
					ii.comma.Print(output, source);
			}

			this->close.Print(output, source);
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);
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

			return CValue(context->root->VoidType, 0);
		}

		virtual Type* TypeCheck(CompilerContext* context)
		{
			return 0;
		}

		void CompileDeclarations(CompilerContext* context)
		{
			if (this->parent->parent != 0)
				context->root->Error("Cannot use typedef outside of global scope", token);

			context->root->ns->members.insert({ this->new_type.text, context->root->LookupType(this->other_type.text, false) });
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
