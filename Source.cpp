#include "Source.h"
#include "Parser.h"

using namespace Jet;

Source::Source(const char* src, const std::string& filename)
{
	this->text = src;
	this->index = 0;
	this->length = strlen(src);
	this->linenumber = 1;
	this->column = 1;
	this->filename = filename;

	this->lines.push_back({ text, 0 });
}

Source::~Source()
{
	delete[] this->text;
}

std::string Source::GetLine(unsigned int line)
{
	auto l = this->lines[line-1];
	if (l.second == 0)
	{
		//search for the end
		while (l.first[l.second] != 0 && l.first[l.second] != '\n')
			l.second++;
	}
	return std::string(l.first, l.second);
}

const char* Source::GetLinePointer(unsigned int line)
{
	auto l = this->lines[line - 1];
	
	return l.first;
}

char Source::ConsumeChar()
{
	if (index >= length)
		return 0;

	column++;
	if (text[index] == '\n')
	{
		this->linenumber++;
		this->column = 0;

		this->lines.push_back({ &text[index+1], 0 });
	}

	return text[index++];
}

char Source::MatchAndConsumeChar(char c)
{
	if (index >= length)
		return 0;

	char ch = text[index];
	if (c == ch)
	{
		column++;
		index++;
		if (ch == '\n')
		{
			this->column = 0;
			this->linenumber++;
			this->lines.push_back({ &text[index+1], 0 });
		}
	}
	return ch;
}

char Source::PeekChar()
{
	if (index >= length)
		return 0;

	return text[index];
}

bool Source::IsAtEnd()
{
	return this->index >= length;
}



extern Source* current_source;
BlockExpression* Source::GetAST(DiagnosticBuilder* builder)
{
	current_source = this;

	Lexer lexer(this, builder);
	Parser parser(&lexer, builder);

	BlockExpression* result = 0;

	try
	{
		result = parser.ParseAll();
	}
	catch (...)
	{

	}

	return result;
}

