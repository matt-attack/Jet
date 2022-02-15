#pragma once

#include "Expression.h"
#include <Token.h>

#include <vector>

namespace Jet
{
    struct FunctionArg
	{
		Token type, name, comma;
	};
    class ScopeExpression;
    class Function;
    class CompilerContext;
    class ExpressionVisitor;
    class NameExpression;
    class Type;
	class FunctionExpression : public Expression
	{
		friend class StructExpression;
		friend struct Namespace;
		friend class Compiler;
		friend class CompilerContext;
		friend class Type;
		friend struct Function;
        friend struct FunctionType;
		friend struct Struct;

		Token name;
		std::vector<FunctionArg>* args;
		std::vector<Token>* captures;
		ScopeExpression* block;
		Token token;
        Token const_tok;

		Token Struct, colons;

		Token ret_type;
		NameExpression* varargs;

		bool is_generator;
		std::vector<std::pair<Token, Token>>* templates;
		Token open_bracket, close_bracket;
		Token oper;

		Function* myself;
	public:

		ScopeExpression* GetBlock()
		{
			return block;
		}

		FunctionExpression(Token token, Token name, Token ret_type, bool generator, std::vector<FunctionArg>* args, ScopeExpression* block, /*NameExpression* varargs = 0,*/ Token Struct, Token colons, std::vector<std::pair<Token, Token>>* templates, std::vector<Token>* captures, Token open_bracket, Token close_bracket, Token oper, Token const_tok)
		{
			this->oper = oper;
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
            this->const_tok = const_tok;
		}

		~FunctionExpression();

		std::string GetFunctionNamePrefix();

		const std::string& GetName()
		{
			return this->name.text;
		}

		void SetParent(Expression* parent);

		virtual Type* TypeCheck(CompilerContext* context);

		CValue Compile(CompilerContext* context);
		void CompileDeclarations(CompilerContext* context);

		void Print(std::string& output, Source* source);

		virtual void Visit(ExpressionVisitor* visitor);

		CValue DoCompile(CompilerContext* context);//call this to do real compilation

        std::pair<const Token*, const Token*> GetTokenRange() const override
        {
            return { &token, 0 };
        }
	};

}
