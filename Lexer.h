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
		//unsigned int index;
		//std::istream* stream;

		//std::string text;
		Source* src;
		//std::vector<std::pair<const char*, unsigned int>> lines;
	public:
		Lexer(Source* source);// std::istream* input, std::string filename);
		//Lexer(std::string text, std::string filename);

		Token Next();

		//std::string GetLine(unsigned int line);

		//std::string filename;

	//private:
		//char ConsumeChar();
		//char MatchAndConsumeChar(char c);
		//char PeekChar();
	};
}
#endif