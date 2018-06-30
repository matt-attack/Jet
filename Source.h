#ifndef JET_SOURCE_HEADER
#define JET_SOURCE_HEADER

#include <string>
#include <vector>
#include <map>

#include <string.h>
#include <stdio.h>

namespace Jet
{
	class DiagnosticBuilder;
	class BlockExpression;
	class Source
	{
		unsigned int index;

		unsigned int length;
		char* text;

		std::vector<std::pair<const char*, unsigned int> > lines;
	public:

		//takes ownership of source
		Source(char* source, const std::string& filename);
		Source(Source&& source)
		{
			this->text = source.text;
			this->index = source.index;
			this->length = source.length;
			this->filename = source.filename;
			this->column = source.column;
			this->linenumber = source.linenumber;
			this->lines = std::move(source.lines);

			source.lines.clear();
			source.text = 0;
		}
		~Source();

		//used for setting up debug info from symbols
		void SetCurrentLine(unsigned int line);

		std::string GetLine(unsigned int line);
		const char* GetLinePointer(unsigned int line);

		void PreProcess(std::map<std::string, bool>& vars, DiagnosticBuilder* diag);

		char EatChar();//do not produce a line from it
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

		int GetIndex()
		{
			return this->index;
		}

		int GetLength()
		{
			return this->length;
		}

		const char* GetSubstring(int start, int end)
		{
			return &this->text[start];
		}

		//throw this in a try catch statement, as it can throw errors
		BlockExpression* GetAST(DiagnosticBuilder* diagnostics, const std::map<std::string, bool>& defines);

		std::string filename;

		//current position in file
		unsigned int linenumber;
		unsigned int column;
	};
}

#endif