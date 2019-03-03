#ifndef JET_PROJECT_HEADER
#define JET_PROJECT_HEADER

#include <string>
#include <vector>
#include <fstream>
#include <map>

namespace Jet
{
	std::string GetNameFromPath(const std::string& path);

	class Source;
	class JetProject
	{
	public:
		struct Element;
		struct Document
		{
			std::map<std::string, Element*> sections;
		};

		struct Element
		{
			std::map<std::string, std::vector<std::string>*> children;
		};
	private:

		bool opened, is_executable;

		JetProject()
		{
			document = 0;
			opened = false;
			version = "0.0.0";//default version for if one is not specified
		}

		bool _Load(const std::string& projectdir);

		std::vector<std::string> resolved_deps;

	public:
		Document* document;
		std::string path;
		std::string project_name;
		std::string version;
		std::vector<std::string> files;
		std::vector<std::string> dependencies;//jet libraries we want to use
		std::vector<std::string> libs;//libraries to link to
		std::vector<std::string> headers;//headers to convert and add to the project
		std::map<std::string, bool> defines;//conditional compilation
		
		~JetProject()
		{
			delete[] this->document;
		}

		struct BuildConfig
		{
			std::string name;
			std::string prebuild, postbuild;
			std::string options;
		};
		std::vector<BuildConfig> configurations;

		bool IsExecutable()
		{
			return this->is_executable;
		}

		
		std::map<std::string, Source*> GetSources();

		static JetProject* Load(const std::string& projectdir);

		const std::vector<std::string>& ResolveDependencies();
	};
}

#endif