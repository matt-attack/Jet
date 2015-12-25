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


Source* Token::GetSource(Compilation* compilation)
{
	for (auto ii : compilation->sources)
	{
		if (ii.second->GetLinePointer(1) <= this->text_ptr && this->text_ptr <= &ii.second->GetLinePointer(1)[ii.second->GetLength()])
		{
			return ii.second;
		}
	}
	return 0;
}