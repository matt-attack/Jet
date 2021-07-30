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
	class DiagnosticBuilder;

	extern std::map<TokenType,std::string> TokenToString; 
	class Lexer
	{
		Source* src;
		int last_index;//location where last token ended, used for tracking whitespace

		DiagnosticBuilder* diag;

		std::vector<std::string> ifstack;

		std::map<std::string, bool> defines;
	public:
		Lexer(Source* source, DiagnosticBuilder* diag, const std::map<std::string, bool>& defines);

		Token Next();
	};
}
#endif