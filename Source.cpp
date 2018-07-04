#include "Source.h"
#include "Parser.h"

using namespace Jet;

Source::Source(char* src, const std::string& filename)
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

void Source::SetCurrentLine(unsigned int line)
{
	if (line > this->lines.size())
	{
		this->lines.resize(line);
	}
	this->lines[line - 1].first = &text[index];
	this->linenumber = line;
	this->column = 0;
}

std::string Source::GetLine(unsigned int line)
{
	auto l = this->lines[line - 1];
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
char Source::EatChar()
{
	if (index >= length)
		return 0;

	column++;

	return text[index++];
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

		if (this->lines.size() == this->linenumber - 1)
		{
			this->lines.push_back({ &text[index + 1], 0 });
		}
		else
		{
			this->lines[this->linenumber - 2] = { &text[index + 1], 0 };
		}
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
			//this->lines.push_back({ &text[index + 1], 0 });
			if (this->lines.size() == this->linenumber - 1)
			{
				this->lines.push_back({ &text[index + 1], 0 });
			}
			else
			{
				this->lines[this->linenumber - 2] = { &text[index + 1], 0 };
			}
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

BlockExpression* Source::GetAST(DiagnosticBuilder* builder, const std::map<std::string, bool>& defines)
{
	Lexer lexer(this, builder, defines);
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

int ProcessBlock(char* text, int length, std::map<std::string, bool>& vars, DiagnosticBuilder* diag)
{
	const char* data = text;
	for (int i = 0; i < length; i++)
	{
		if (text[i] == '#')
		{
			//its preprocessor stuff
			//read the variable, check the condition, then replace line with ' '
			//and following lines with ' ' if not applied
			if (strncmp(text + i + 1, "if ", 3) == 0)
			{
				i += 4;

				std::string condition;
				for (; i < length; i++)
				{
					if (text[i] == '\n' || text[i] == '\r')//read in the line
						break;
					condition += text[i];
				}

				bool is_true = vars.find(condition) != vars.end();

				//erase the line
				memset(text, ' ', i);

				//check if condition is true or false, if false read to end or else then stop
				if (is_true == false)
				{
					//skip over block until else or next condition
					for (; i < length; i++)
					{
						if (text[i] == '#')
						{
							if (strncmp(text + i + 1, "else", 4) == 0)
							{
								//erase the else
								memset(text + i, ' ', 5);

								i += 5;

								//skip over until endif, 
								for (; i < length; i++)
								{
									if (text[i] == '#')
									{
										if (strncmp(text + i + 1, "if ", 3) == 0)
											i += ProcessBlock(&text[i], length, vars, diag);
										else if (strncmp(text + 1 + i, "endif", 5) == 0)
										{
											//erase the endif and go to next line
											char* cur = &text[i];

											memset(text + i, ' ', 6);
											return i;
										}
										else
										{
											diag->Error("Unexpected # statement", Token());
											throw 7;
										}
									}
								}

								break;
							}
							else if (strncmp(text + i + 1, "elseif", 5) == 0)
							{
								i += 6;
								diag->Error("Elseif Not Implemented!", Token());
								throw 7;
								break;

								//parse the else
							}
							else if (strncmp(text + i + 1, "endif", 5) == 0)
							{
								memset(text + i, ' ', 6);
								i += 6;
								
								//parse to end of line then return
								char* c = &text[i];

								return i;
							}
						}
						else
							if (text[i] != '\r' && text[i] != '\n')
								text[i] = ' ';
					}
					if (i == length)
					{
						diag->Error("Missing end if", Token());
						throw 7;
					}
					return i;
				}
				else//works
				{
					//read over to next line then

					//read until an if or stop otherwise
					for (; i < length; i++)
					{
						if (text[i] == '#')
						{
							if (strncmp(text + i + 1, "if ", 3) == 0)
								i += ProcessBlock(&text[i], length, vars, diag);
							else if (strncmp(text + 1 + i, "endif", 5) == 0)
							{
								//erase the endif and go to next line
								char* cur = &text[i];

								memset(text + i, ' ', 6);
								break;
							}
							else if (strncmp(text + 1 + i, "else", 4) == 0)
							{
								//write over the else
								memset(text + i, ' ', 5);

								i += 5;
								//skip over until endif, 
								for (; i < length; i++)
								{
									if (text[i] == '#')
									{
										if (strncmp(text + 1 + i, "endif", 5) == 0)
										{
											//erase the endif and go to next line
											char* cur = &text[i];

											memset(text + i, ' ', 6);
											break;
										}
									}
									if (text[i] != '\r' && text[i] != '\n')
										text[i] = ' ';
								}
								break;
							}
							else
							{
								diag->Error("Unexpected # statement", Token());
								throw 7;
							}
						}
					}
					return i;
				}
			}
			else
			{
				diag->Error("Unexpected # statement", Token());
				throw 7;
			}
		}
	}
	diag->Error("Missing #endif", Token());
	throw 7;
}

void Source::PreProcess(std::map<std::string, bool>& vars, DiagnosticBuilder* diag)
{
	//read until an if or stop otherwise
	for (unsigned int i = 0; i < this->length; i++)
	{
		if (this->text[i] == '#')
		{
			if (strncmp(text + i + 1, "if ", 3) == 0)
				i += ProcessBlock(&this->text[i], this->length, vars, diag);
			else
			{
				diag->Error("Unexpected # statement", Token(text, 0, 1, 0, TokenType::AddAssign, "#"));
				throw 7;
			}
		}
	}
}