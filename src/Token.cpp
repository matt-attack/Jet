#include "Token.h"
#include "Source.h"
#include "Compiler.h"

using namespace Jet;

void Token::Print(std::string& str, Source* source) const
{
	//output trivia
	auto code = this->text_ptr;
	auto trivia = this->text_ptr - this->trivia_length;
	for (int i = 0; i < this->trivia_length; i++)
		str += trivia[i];
	
	str += text;
}

Source* Token::GetSource(Compilation* compilation) const 
{
	for (auto ii : compilation->sources)
	{
		const char* data = ii.second->GetSubstring(0, 0);
		if (data <= this->text_ptr && this->text_ptr <= &data[ii.second->GetLength()])
		{
			return ii.second;
		}
	}
	return 0;
}