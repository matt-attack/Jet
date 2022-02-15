#ifndef JET_DIAGNOSTIC_HEADER
#define JET_DIAGNOSTIC_HEADER

#include "Token.h"

#include <string>
#include <vector>
#include <functional>

namespace Jet
{
    enum DiagnosticSeverity
    {
        ERROR = 0,
        WARNING = 1,
        INFO = 2
    };

	struct Diagnostic
	{
		std::string message;
		std::string file;

		std::string line;//the line of code that the function was on
		Token token;
		Token end;//optional

		DiagnosticSeverity severity;//0 = error, 1 = warning, 2 = info ect..
		void Print();
	};

    inline std::string BOLD(const std::string& str)
    {
        return "\x1b[1m" + str + "\x1b[0m";
        //return str;
    }

//#define BOLD(x) "test"##x"test"

	class DiagnosticBuilder
	{
		friend class Compilation;
		std::vector<Diagnostic> diagnostics;
		std::function<void(Diagnostic&)> callback;
		Compilation* compilation;
	public:

		DiagnosticBuilder(std::function<void(Diagnostic&)> callback) : callback(callback)
		{

		}

		void Error(const std::string& text, const Token& start, const Token& end, DiagnosticSeverity severity = ERROR);
		void Error(const std::string& text, const Token& token, DiagnosticSeverity severity = ERROR);
		void Error(const std::string& text, const std::string& file, int line = -1, DiagnosticSeverity severity = ERROR);

		std::vector<Diagnostic>& GetErrors()
		{
			return diagnostics;
		}
	};
}

#endif

