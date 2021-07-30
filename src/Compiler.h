#ifndef JET_COMPILER_HEADER
#define JET_COMPILER_HEADER

/*#ifdef _DEBUG
#ifndef DBG_NEW
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#define new DBG_NEW
#endif

#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif*/

#include "types/Types.h"
#include "Token.h"
#include "Compilation.h"

class OptionParser;

namespace Jet
{
	//void ParserError(const std::string& msg, Token token);//todo

	struct CompilerOptions
	{
		int optimization;//from 0-3
		int debug;//0 = none, 1 = lines, 2 = everything
		bool force, run = false, time = false;
		bool output_ir = false;
		std::string target, linker;
		CompilerOptions()
		{
			this->force = false;
			this->optimization = 0;
		}

		void ApplyOptions(OptionParser* parser);
	};

	//todo: why is this a class?
	class Compiler
	{
		void UpdateProjectList(const JetProject* project);

	public:

		static std::string FindProject(const std::string& name, const std::string& desired_version);


		struct ProjectInfo
		{
			std::string name;
			std::string path;
			std::string version;
		};
		static std::vector<ProjectInfo> GetProjectList();

		//returns if was successful, errored, or if there was a rebuild
		//0 = error, 1 = success, 2 = successful, was recompiled
		int Compile(const JetProject* project, const CompilerOptions* options = 0, const std::string& config = "");
	};


	void SetupDefaultCommandOptions(OptionParser* parser);
};

#endif
