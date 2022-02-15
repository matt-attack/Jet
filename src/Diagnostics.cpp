#include "Diagnostics.h"

#include "Source.h"

using namespace Jet;

void Diagnostic::Print()
{
    std::string color = "";
    std::string color_end = "";
#ifndef _WIN32
    color = "\x1B[1;31m";
    color_end = "\x1B[0m";
	const char* m_type = "\x1B[1;31merror\x1B[0m";
    if (this->severity == INFO)
    {
        color = "\x1B[1;34m";
        m_type = "\x1B[1;34minfo \x1B[0m";
    }
#else
	const char* m_type = "error";
#endif
	if (token.type != TokenType::InvalidToken)
	{
		unsigned int startrow = token.column;
		unsigned int endrow = token.column + token.text.length();

		// Handle an end token
		if (end.type != TokenType::InvalidToken)
		{
			endrow = end.column + end.text.length();
		}

		std::string code;
		std::string underline = color;
		for (unsigned int i = 0; i < this->line.length(); i++)
		{
            if (i == startrow)
            {
                code += color;
            }
            code += this->line[i];
            if (i == endrow - 1)
            {
                code += color_end;
            }

			if (code[i] == '\t')
				underline += '\t';
			else if (i >= startrow && i < endrow)
				underline += '~';
			else
				underline += ' ';
		}
        underline += color_end;

        std::string filename = "\x1b[1m" + file + "\x1b[0m";
		printf("[%s] %s %d:%d to %d:%d: %s\n", m_type, filename.c_str(), token.line, startrow, token.line, endrow, message.c_str());
		printf("[%s] >>>%s\n", m_type, code.c_str());
		printf("[%s] >>>%s\n\n", m_type, underline.c_str());
	}
	else
	{
		//just print something out, it was probably a build system error, not one that occurred in the code
		printf("[%s] %s: %s\n", m_type, this->file.c_str(), message.c_str());
	}
}

void DiagnosticBuilder::Error(const std::string& text, const Token& token, DiagnosticSeverity severity)
{
	Diagnostic error;
	error.token = token;
	error.message = text;

	if (token.type != TokenType::InvalidToken)
	{
		auto current_source = token.GetSource((Compilation*)compilation);
		error.line = token.GetLineText(current_source->GetData());//current_source->GetLine(token.line);
		error.file = current_source->filename;
	}
	error.severity = severity;

	if (this->callback)
		this->callback(error);

	//try and remove exceptions from build system
	this->diagnostics.push_back(error);
}

void DiagnosticBuilder::Error(const std::string& text, const Token& start_token, const Token& end_token,
DiagnosticSeverity severity)
{
	Diagnostic error;
	error.token = start_token;
	error.end = end_token;
	error.message = text;

	if (start_token.type != TokenType::InvalidToken)
	{
		auto current_source = start_token.GetSource((Compilation*)compilation);

		error.line = start_token.GetLineText(current_source->GetData());//current_source->GetLine(start_token.line);
		error.file = current_source->filename;
	}
	error.severity = severity;

	if (this->callback)
		this->callback(error);

	//try and remove exceptions from build system
	this->diagnostics.push_back(error);
}


void DiagnosticBuilder::Error(const std::string& text, const std::string& file, int line, DiagnosticSeverity severity)
{
	Diagnostic error;
	error.token = Token();
	error.message = text;

	error.line = line;
	error.file = file;

	error.severity = severity;

	if (this->callback)
		this->callback(error);

	//try and remove exceptions from build system
	this->diagnostics.push_back(error);
}
