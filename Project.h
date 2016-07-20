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
		}

		bool _Load(const std::string& projectdir);

	public:
		Document* document;
		std::string path;
		std::string project_name;
		std::vector<std::string> files;
		std::vector<std::string> dependencies;
		std::vector<std::string> libs;//libraries to link to
		
		~JetProject()
		{
			delete[] this->document;
		}

		struct BuildConfig
		{
			std::string prebuild, postbuild;
			std::string options;
		};
		std::map<std::string, BuildConfig> configurations;

		bool IsExecutable()
		{
			return this->is_executable;
		}

		
		std::map<std::string, Source*> GetSources();

		static JetProject* Load(const std::string& projectdir)
		{
			JetProject* p = new JetProject;
			p->_Load(projectdir);
			if (p->opened)
				return p;
			else
				delete p;
			return 0;
		}
	};
}

#endif