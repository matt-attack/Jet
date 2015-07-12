#ifndef _JET_LEXER_HEADER
#define _JET_LEXER_HEADER

#include "Token.h"
#include <map>
#include <vector>

namespace Jet
{
	bool IsLetter(char c);
	bool IsNumber(char c);

	class Source;

	extern std::map<TokenType,std::string> TokenToString; 
	class Lexer
	{
		Source* src;
		int last_index;//location where last token ended, used for tracking whitespace

	public:
		Lexer(Source* source);

		Token Next();
	};
}
#endif