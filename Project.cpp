#include "Project.h"
#include "Source.h"

#include "Compiler.h"

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#endif

using namespace Jet;

std::string Jet::GetNameFromPath(const std::string& path)
{
	std::string name;
	bool first = false;
	int i = path.length() - 1;
	for (; i >= 0; i--)
	{
		if (path[i] == '/' || path[i] == '\\')
		{
			if (first)
				break;
			first = true;
		}
		else if (first == false)
		{
			first = true;
		}
	}
	if (i < 0)
		i = 0;

	for (; i < path.length(); i++)
	{
		if (path[i] != '/' && path[i] != '\\')
			name.push_back(path[i]);
	}

	return name;
}



JetProject::Document* ParseConfig(std::ifstream& pf)
{
	JetProject::Document* e = new JetProject::Document;
	auto cur = new JetProject::Element;
	//	use sections like
	//	[debug]
	//thing: a
	//rather than{}
	std::vector<std::string>* cur_item = 0;
	e->sections[""] = cur;
	while (pf.peek() != EOF)
	{
		std::string file;//read in a filename
		bool is_escaped = false;
		while ((is_escaped || pf.peek() != ' ') && pf.peek() != EOF && pf.peek() != '\r' && pf.peek() != '\n' && pf.peek() != '\t')
		{
			if (pf.peek() == '"')
			{
				is_escaped = !is_escaped;
				pf.get();
			}
			else
				file += pf.get();//handle escaping
		}

		pf.get();

		if (file.length() == 0)
		{

		}
		else if (file.back() == ':')
		{
			cur_item = new std::vector < std::string > ;
			cur->children[file.substr(0, file.length() - 1)] = cur_item;
		}
		else if (file.front() == '[' && file.back() == ']')
		{
			cur = new JetProject::Element;
			e->sections[file.substr(1, file.length() - 2)] = cur;
		}
		else
		{
			if (cur_item)
				cur_item->push_back(file);
			else
			{
				printf("Error: no current key");
				throw 7;
			}
		}
	}
	
	return e;
}

#ifdef _WIN32
#include <Windows.h>
#endif
void GetFilesInDirectory(std::vector<std::string> &out, const std::string &directory)
{
#ifdef _WIN32
	HANDLE dir;
	WIN32_FIND_DATAA file_data;

	if ((dir = FindFirstFileA((directory + "/*").c_str(), &file_data)) == INVALID_HANDLE_VALUE)
		return; /* No files found */

	do {
		const std::string file_name = file_data.cFileName;
		const std::string full_file_name = directory + "/" + file_name;
		const bool is_directory = (file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

		if (file_name[0] == '.')
			continue;

		if (is_directory)
			continue;

		out.push_back(file_name);
	} while (FindNextFileA(dir, &file_data));

	FindClose(dir);
#else
	DIR *dir;
	class dirent *ent;
	class stat st;

	dir = opendir(directory.c_str());
	while ((ent = readdir(dir)) != NULL) {
		const std::string file_name = ent->d_name;
		const std::string full_file_name = directory + "/" + file_name;

		if (file_name[0] == '.')
			continue;

		if (stat(full_file_name.c_str(), &st) == -1)
			continue;

		const bool is_directory = (st.st_mode & S_IFDIR) != 0;

		if (is_directory)
			continue;

		out.push_back(file_name);
	}
	closedir(dir);
#endif
} // GetFilesInDirectory

bool JetProject::_Load(const std::string& projectdir)
{
	//ok, lets parse the jp file
	std::string name = projectdir;
	if (projectdir.length() > 0 && projectdir.back() != '/')
		name += '/';
	std::ifstream pf(name + "project.jp", std::ios::in | std::ios::binary);
	if (pf.is_open() == false)
	{
		printf("Error: Could not find project file %s/project.jp\n", projectdir.c_str());
		return false;
	}

	this->path = projectdir;

	char olddir[500];
	getcwd(olddir, 500);

	std::string realpath = olddir;
	realpath += '\\';
	realpath += projectdir;

	this->is_executable = true;
	int current_block = 0;
	auto doc = ParseConfig(pf);
	auto root = doc->sections[""];
	if (root->children.find("lib") != root->children.end() 
		&& root->children.find("lib")->second->size() > 0)
	{
		this->project_name = root->children["lib"]->at(0);
		is_executable = false;
	}
	else if (root->children.find("bin") != root->children.end()
		     && root->children.find("bin")->second->size() > 0)
	{
		this->project_name = root->children["bin"]->at(0);
		is_executable = true;
	}
	else
	{
		// we didnt have a project name, fail
		printf("Error: Project didn't have a name. Add a bin or lib tag with the project name.\n");
		return false;
	}

	// error if we have a bin and lib tag
	if (root->children.find("lib") != root->children.end()
		&& root->children.find("bin") != root->children.end())
	{ 
		printf("Error: Project cannot be both a bin and a library. Remove either the bin or the lib tag.\n");
		return false;
	}

	if (root->children.find("version") != root->children.end()
		&& root->children.find("version")->second->size() > 0)
		this->version = root->children["version"]->at(0);

	if (root->children["files"])
		this->files = *root->children["files"];

	if (root->children["libs"])
		this->libs = *root->children["libs"];

	if (root->children["requires"])
		this->dependencies = *root->children["requires"];

	if (root->children["headers"])
		this->headers = *root->children["headers"];

	if (root->children["defines"])
	{
		for (auto ii : *root->children["defines"])
		{
			defines[ii] = true;
		}
	}

	for (auto ii : doc->sections)
	{
		if (ii.first.length() > 0)
		{
			std::string pre, post, config;
			if (ii.second->children["prebuild"] && ii.second->children["prebuild"]->size() > 0)
				pre = ii.second->children["prebuild"]->front();
			if (ii.second->children["postbuild"] && ii.second->children["postbuild"]->size() > 0)
				post = ii.second->children["postbuild"]->front();
			if (ii.second->children["config"] && ii.second->children["config"]->size() > 0)
				config = ii.second->children["config"]->front();
			this->configurations.push_back({ ii.first, pre, post, config });//this->configurations[ii.first] = { pre, post, config };
		}
	}

	delete doc;
	if (files.size() == 0)
	{
		//if no files, then just add all.jet files in the directory
		std::vector<std::string> files_found;
		GetFilesInDirectory(files_found, projectdir);

		for (auto ii : files_found)
		{
			if (ii.substr(ii.length() - 4) == ".jet")
				files.push_back(ii);
		}
	}
	this->opened = true;
	return true;
}

std::map<std::string, Source*> JetProject::GetSources()
{
	//char cwd[500];
	//getcwd(cwd, 500);
	std::map<std::string, Source*> sources;
	//std::string scwd = cwd;
	//int p = scwd.find_last_of('\\');
	std::string project = this->project_name + "/";
	for (auto file : files)
	{
		std::ifstream t(file, std::ios::in | std::ios::binary);
		if (t)
		{
			t.seekg(0, std::ios::end);    // go to the end
			std::streamoff length = t.tellg();           // report location (this is the length)
			t.seekg(0, std::ios::beg);    // go back to the beginning
			char* buffer = new char[length + 1];    // allocate memory for a buffer of appropriate dimension
			t.read(buffer, length);       // read the whole file into the buffer
			buffer[length] = 0;
			t.close();

			sources[file] = new Source(buffer, project + file);
		}
		else
		{
			sources[file] = 0;
		}
	}
	return sources;
}

const std::vector<std::string>& JetProject::ResolveDependencies()
{
	if (this->resolved_deps.size())
		return this->resolved_deps;

	for (auto dep : this->dependencies)
	{
		if (dep[0] != '.' && dep.find('/') == -1 && dep.find('\\') == -1)
		{
			//ok, search for the p/ackage using our database, we didnt give a path to one
			std::string path = Compiler::FindProject(dep, "0.0.0");//todo, can optimize this so we dont read the file n times lolol
			
			//if (path.length() == 0)
			//{
				// we failed to find this project
			//	printf("Project \"%s\" was not resolved.\nMake sure to build it first so the compiler knows where to find it.", dep.c_str());
			//}

			//printf("Project \"%s\" resolved to %s\n", dep.c_str(), path.c_str());

			//if we couldnt find it there will just be a blank string that we can handle down later
			this->resolved_deps.push_back(path);
		}
		else
		{
			this->resolved_deps.push_back(dep);
		}
	}
	return this->resolved_deps;
}

JetProject* JetProject::Load(const std::string& projectdir)
{
	JetProject* p = new JetProject;
	p->_Load(projectdir);
	if (p->opened)
		return p;
	else
		delete p;
	return 0;
}