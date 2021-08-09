#pragma once

#include <Token.h>

#include <expressions/Expressions.h>

namespace Jet
{
	class Source;
	class Compiler;

	class LetExpression : public Expression
	{
	public:
		struct TypeNamePair
		{
			Token type;
			Token name;
		};

	private:
		std::vector<TypeNamePair>* _names;
		std::vector<std::pair<Token, Expression*>>* _right;

		bool is_const;
		Token token;
        Token equals;

    public:

		LetExpression(Token token, Token equals, std::vector<TypeNamePair>* names, std::vector<std::pair<Token, Expression*>>* right)
		{
			this->equals = equals;
			this->token = token;
			this->_names = names;
			this->_right = right;

			is_const = (token.type == TokenType::Const);
		}

		~LetExpression()
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
			for (auto& ii : *this->_names)
			{
				if (ii.type.text.length())
				{
					context->CurrentToken(&ii.type);
					context->TCRegisterLocal(ii.name.text, context->root->LookupType(ii.type.text, false)->GetPointerType());
				}
				else
				{
					//type inference
					auto ty = (*this->_right)[i].second->TypeCheck(context);
					context->TCRegisterLocal(ii.name.text, ty->GetPointerType());
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
				if (ii.type.text.length() > 0)
					ii.type.Print(output, source);
				ii.name.Print(output, source);
			}

			if (this->_right)
				this->equals.Print(output, source);

			unsigned int i = 0;
			for (auto ii : *_names)
			{
				if (_right && i < _right->size())
				{
					if ((*_right)[i].first.text.length())
						(*_right)[i].first.Print(output, source);
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
}
