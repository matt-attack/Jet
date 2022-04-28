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

    struct FunctionSignatureData
    {
        Token name;
        Token colons;
        Token struct_name;
        Token return_type;
        Token open_paren, close_paren;
        std::vector<FunctionArg>* arguments = 0;
        Token operator_token;

        std::vector<std::pair<Token, Token>>* templates = 0;
    };

    class ScopeExpression;
    struct FunctionParsedData
    {
        Token token;
        //Token name;
        //Token return_type;

        FunctionSignatureData signature;

        bool is_generator;
        bool is_static;
        //std::vector<FunctionArg>* arguments = 0;
        ScopeExpression* block = 0;
        //Token Struct;
        Token colons;
        std::vector<Token>* captures = 0;

        //std::vector<std::pair<Token, Token>>* templates = 0;
        //Token open_bracket, close_bracket;
        //Token operator_tok;
        Token constant_token;
    };

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

		/*Token name;
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
		Token oper;*/

        FunctionParsedData data_;

		Function* myself;
	public:

		ScopeExpression* GetBlock()
		{
			return data_.block;
		}

        FunctionExpression(const FunctionParsedData& data)
        {
            data_ = data;
        }

		~FunctionExpression();

		std::string GetFunctionNamePrefix();

		const std::string& GetName()
		{
			return data_.signature.name.text;
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
            return { &data_.token, 0 };
        }
        
    private:
    
    	// instantiates the Function* myself for this
    	void CreateFunction(CompilerContext* context, bool advance_lookup);
	};

}
