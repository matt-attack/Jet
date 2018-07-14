#pragma once

class OptionVar
{
	std::string name;
	std::string value;
	bool take_value;
public:
	OptionVar()
	{

	}

	OptionVar(const char* name, const char* def, bool take_value)
	{
		this->name = name;
		this->value = def;
		this->take_value = take_value;
	}

	bool ShouldTake()
	{
		return this->take_value;
	}

	bool GetBool()
	{
		return false;
	}

	int GetInt()
	{
		if (this->value.length() == 0)
			return 0;
		try
		{
			return std::stoi(this->value);
		}
		catch (...)
		{
			return 0;
		}
	}

	std::string GetString()
	{
		return value;
	}

	void SetValue(const std::string& value)
	{
		this->value = value;
	}
};

class OptionParser
{
	std::map<std::string, OptionVar> vars;

public:
	std::vector<std::string> commands;

	OptionVar& GetOption(const char* name)
	{
		return vars[name];
	}

	void AddOption(const char* name, const char* def, bool take_value = true)
	{
		if (vars.find(name) == vars.end())
			vars[name] = OptionVar(name, def, take_value);
	}

	void Parse(const std::string& aargs)
	{
		char* command = new char[aargs.size()+1];
		strcpy(command, aargs.c_str());
		char* args[400] = {};
		int numargs = 0;
		int i = 0; bool inquotes = false;

		//add a dummy arg with the current location
		numargs = 1;
		args[0] = "~ignore me~";

		while (command[i])
		{
			if (command[i] == '"')
			{
				inquotes = !inquotes;
				if (inquotes == false)
					command[i] = 0;
			}
			else if (command[i] == ' ' && inquotes == false)
			{
				command[i] = 0;

				numargs++;
			}
			else
			{
				if (args[numargs] == 0)
					args[numargs] = &command[i];
			}
			i++;
		}
		if (args[numargs] != 0)
			numargs++;

		this->Parse(numargs, args);
		delete[] command;
	}

	void Parse(int argc, char ** args)
	{
		for (int i = 1; i < argc; i++)
		{
			if (args[i][0] == '-' && args[i][1] == '-')
			{
				std::string option = &args[i][2];
				
				std::string value;
				if (i + 1 < argc)
					value = args[i+1];

				auto find = vars.find(option);
				if (find != vars.end())
				{
					if (vars[option].ShouldTake())
						i++;
					else
						value = true;
					vars[option].SetValue(value);
				}
				else
				{
					printf("WARNING: Argument/flag '%s' does not exist\n", option.c_str());
					vars[option] = OptionVar(option.c_str(), value.c_str(), true);
				}
			}
			else if (args[i][0] == '-')
			{
				//its an arg
				char o = args[i][1];
				std::string option;
				option += o;

				//read in value
				std::string value;
				int pos = 2;
				while (args[i][pos])
					value += args[i][pos++];

				auto find = vars.find(option);
				if (find != vars.end())
					vars[option].SetValue(value);
				else
				{
					printf("WARNING: Argument/flag '%s' does not exist\n", option.c_str());
					vars[option] = OptionVar(option.c_str(), value.c_str(), true);
				}
			}
			else
			{
				//read in value
				std::string value;

				int pos = 0;
				while (args[i][pos])
					value += args[i][pos++];

				this->commands.push_back(value);
			}
		}
	}
};