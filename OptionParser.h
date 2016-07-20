#pragma once

class OptionVar
{
	const char* name;
	std::string value;
public:
	OptionVar()
	{

	}

	OptionVar(const char* name, const char* def)
	{
		this->name = name;
		this->value = def;
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

	void AddOption(const char* name, const char* def)
	{
		if (vars.find(name) == vars.end())
			vars[name] = OptionVar(name, def);
	}

	void Parse(const std::string& args)
	{
		//just parse it into an array of space separated things to process below
		int pos = 0;
		while (pos < args.length())
		{
			char c = args[pos++];
			if (c == '-' && pos < args.length())//its an option
			{
				char o = args[pos++];
				std::string option;
				option += o;

				//read in value
				std::string value;
				while (args[pos] && args[pos] != ' ')
					value += args[pos++];

				pos++;

				auto find = vars.find(option);
				if (find != vars.end())
					vars[option].SetValue(value);
			}
			else
			{
				//read in value
				std::string value;
				value += c;
				while (args[pos] && args[pos] != ' ')
					value += args[pos++];

				pos++;
				this->commands.push_back(value);
			}
		}
	}

	void Parse(int argc, char ** args)
	{
		for (int i = 1; i < argc; i++)
		{
			if (args[i][0] == '-')
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