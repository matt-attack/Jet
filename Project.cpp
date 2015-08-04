#include "Project.h"
#include "Source.h"

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

bool JetProject::_Load(const std::string& projectdir)
{
	//ok, lets parse the jp file
	std::ifstream pf(std::string(projectdir) + "/project.jp", std::ios::in | std::ios::binary);
	if (pf.is_open() == false)
	{
		printf("Error: Could not find project file %s/project.jp\n", projectdir.c_str());
		return false;
	}

	this->path = projectdir;

	is_executable = true;
	int current_block = 0;
	project_name = GetNameFromPath(projectdir);
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

		if (file == "files:")
		{
			current_block = 1;
			continue;
		}
		else if (file == "requires:")
		{
			current_block = 2;
			continue;
		}
		else if (file == "lib:")
		{
			current_block = 3;
			is_executable = false;
			continue;
		}
		else if (file == "libs:")
		{
			current_block = 4;
			continue;
		}
		else if (file == "\n" || file == "")
		{
			continue;
		}

		switch (current_block)
		{
		case 1:
			if (file.length() > 0)
				files.push_back(file);
			break;
		case 2:
			if (file.length() > 0)
				dependencies.push_back(file);
			break;//do me later
		case 3:
			break;
		case 4:
			if (file.length() > 0)
				libs.push_back(file);
			break;
		default:
			printf("Malformatted Project File!\n");
		}
	}

	if (files.size() == 0)
	{
		//if no files, then just add all.jet files in the directory
	}
	this->opened = true;
	return true;
}

std::map<std::string, Source*> JetProject::GetSources()
{
	std::map<std::string, Source*> sources;
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

			sources[file] = new Source(buffer, file);
		}
		else
		{
			sources[file] = 0;
		}
	}
	return sources;
}