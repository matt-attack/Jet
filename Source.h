#ifndef JET_SOURCE_HEADER
#define JET_SOURCE_HEADER

#include <string>
#include <vector>

namespace Jet
{
	class Source
	{
		unsigned int index;

		unsigned int length;
		const char* text;

		std::vector<std::pair<const char*, unsigned int> > lines;
	public:

		//takes ownership
		Source(const char* source, const std::string& filename);
		~Source();

		std::string GetLine(unsigned int line);

		char ConsumeChar();
		char MatchAndConsumeChar(char c);
		char PeekChar();

		bool IsAtEnd();
		
		unsigned int Remaining()
		{
			return length + 1 - index;
		}

		bool IsOperator(const std::string& op)
		{
			return memcmp(op.c_str(), &text[index - 1], op.length()) == 0;
		}

		std::string filename;

		//current position in file
		unsigned int linenumber;
		unsigned int column;
	};
}

#endif