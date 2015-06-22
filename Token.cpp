#include "Token.h"
#include "Source.h"

using namespace Jet;

void Token::Print(std::string& str, Source* source)
{
	//output trivia
	auto code = this->text_ptr;
	auto trivia = this->text_ptr - this->trivia_length;
	for (int i = 0; i < this->trivia_length; i++)
	{
		str += trivia[i];
	}
	str += text;//do more later lel
}