
#ifndef _PARSER
#define _PARSER

#include <stdio.h>
#include <map>

#include "Token.h"
//#include "Compiler.h"
#include "Expressions.h"
#include "Parselets.h"
#include "Lexer.h"
//#include "JetExceptions.h"

#include <deque>

namespace Jet
{
	class Parser
	{
		Lexer* lexer;
		std::map<TokenType, InfixParselet*> mInfixParselets;
		std::map<TokenType, PrefixParselet*> mPrefixParselets;
		std::map<TokenType, StatementParselet*> mStatementParselets;
		std::deque<Token> mRead;

	public:
		std::string filename;
		Parser(Lexer* l);

		~Parser();

		Expression* ParseExpression(int precedence = 0);
		Expression* ParseStatement(bool takeTrailingSemicolon = true);//call this until out of tokens (hit EOF)
		BlockExpression* ParseBlock(bool allowsingle = true);
		BlockExpression* ParseAll();

		int GetPrecedence();

		Token Consume();
		Token Consume(TokenType expected);

		Token ConsumeTemplateGT();

		Token LookAhead(unsigned int num = 0);

		bool Match(TokenType expected);
		bool MatchAndConsume(TokenType expected);

		void Register(TokenType token, InfixParselet* parselet);
		void Register(TokenType token, PrefixParselet* parselet);
		void Register(TokenType token, StatementParselet* parselet);
	};
}
#endif