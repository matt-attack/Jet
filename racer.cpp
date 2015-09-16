
/*#ifdef _DEBUG
#ifndef DBG_NEW
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#define new DBG_NEW
#endif

#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif*/

#include <stdio.h>

#include <stack>
#include <vector>

#define CODE(code) #code

#include "Project.h"
#include "Compiler.h"
#include "Expressions.h"
#include "Types\Function.h"

#include <iostream>
#include <string>
#include <fstream>
#include <streambuf>
#include <math.h>
#include <functional>

#include <windows.h>
using namespace Jet;


#include <sstream>

extern "C"
{
	/*__declspec(dllexport) const char* GetFunction(const char* ffile, int line)
	{
		//a little test
		//return "hello";
		OutputDebugString(ffile);
		//return ffile;
		auto project = JetProject::Load("C:/users/matthew/desktop/vm/asmvm2/asmvm/jetcore");
		auto compilation = Compilation::Make(project);
		//compilation->Assemble(0);

		//lets make a line# to function translator
		std::string file = ffile;
		for (auto ii : compilation->functions)
		{
			if (ii.second->expression)
			{
				auto block = ii.second->expression->GetBlock();
				if (block->start.line <= line && block->end.line >= line)
				{
					auto src = block->start.GetSource(compilation);
					if (src->filename == file)
					{
						printf("found it");
						return ii.second->name.c_str();
						break;
					}
				}
			}
		}

		for (auto ty : compilation->types)
		{
			if (ty.second->type == Types::Struct)
			{
				for (auto ii : ty.second->data->functions)
				{
					if (ii.second->expression)
					{
						auto block = ii.second->expression->GetBlock();
						if (block->start.line <= line && block->end.line >= line)
						{
							auto src = block->start.GetSource(compilation);
							if (src->filename == file)
							{
								printf("found it");
								return ii.second->name.c_str();
								break;
							}
						}
					}
				}
			}
			else if (ty.second->type == Types::Trait)
			{
				for (auto ii : ty.second->trait->extension_methods)
				{
					if (ii.second->expression)
					{
						auto block = ii.second->expression->GetBlock();
						if (block->start.line <= line && block->end.line >= line)
						{
							auto src = block->start.GetSource(compilation);
							if (src->filename == file)
							{
								printf("found it");
								return ii.second->name.c_str();
								break;
							}
						}
					}
				}
			}
		}
		return 0;
	}*/

	std::string GetDirectoryFromPath(const char* path)
	{
		std::string out;
		int pos = strlen(path);
		for (; pos >= 0; pos--)
		{
			if (path[pos] == '\\' || path[pos] == '/')
				break;
		}

		for (int i = 0; i < pos; i++)
			out += path[i];
		return out;
	}

	char data[5000];
	__declspec(dllexport) const char* GetSymbolInfo(const char* proj, const char* ffile, const char* symbol, int line)
	{
		auto path = GetDirectoryFromPath(proj);

		auto project = JetProject::Load(path);
		if (project == 0)
			return "";//project didnt load, just return 0

		auto compilation = Compilation::Make(project);

		//lets just check types for now
		auto res = compilation->TryLookupType(symbol);
		if (res != 0)
		{
			strcpy(data, res->ToString().c_str());
			delete compilation;
			delete project;
			return data;
		}

		Function* f = compilation->GetFunctionAtPoint(ffile, line);
		if (f && f->expression)
		{
			Scope* scope = f->expression->GetBlock()->scope;
			for (auto ii : scope->named_values)
			{
				if (ii.first == symbol)
				{
					if (ii.second.type->type == Types::Pointer)
					{
						strcpy(data, ii.second.type->base->ToString().c_str());

						delete compilation;
						delete project;

						return data;
					}
					else
					{
						strcpy(data, ii.second.type->ToString().c_str());
						delete compilation;
						delete project;
						return data;
					}
				}
			}

			if (scope->prev)
			{
				for (auto ii : scope->prev->named_values)
				{
					if (ii.first == symbol)
					{
						if (ii.second.type->type == Types::Pointer)
						{
							strcpy(data, ii.second.type->base->ToString().c_str());

							delete compilation;
							delete project;

							return data;
						}
						else
						{
							strcpy(data, ii.second.type->ToString().c_str());

							delete compilation;
							delete project;

							return data;
						}
					}
				}
			}
		}

		//look for global functions
		auto fun = compilation->ns->members.find(symbol);// functions.find(symbol);
		if (fun != compilation->ns->members.end())
		{
			if (fun->first == symbol && fun->second.type == SymbolType::Function)
			{
				strcpy(data, fun->second.fn->GetType(compilation)->ToString().c_str());

				delete compilation;
				delete project;

				return data;
			}
		}

		delete compilation;
		delete project;

		return "";
	}

	
	__declspec(dllexport) const char* GetAutoCompletes(const char* proj, const char* ffile, int line)
	{
		auto path = GetDirectoryFromPath(proj);
		
		auto project = JetProject::Load(path);
		if (project == 0)
			return "";//project didnt load, just return 0

		auto compilation = Compilation::Make(project);

		std::string out;
		for (auto ii : compilation->ns->members)
		{
			if (ii.second.type == SymbolType::Function)// && ii.second.fn->expression)
			{
				out += ii.first + "F/";
				out += "fun " + ii.second.fn->return_type->ToString() + " " + ii.second.fn->name;
				out += "(";
				bool first = false;
				for (int i = 1; i < ii.second.fn->arguments.size(); i++)
				{
					if (first)
						out += ",";
					else
						first = true;

					out += ii.second.fn->arguments[i].first->ToString() + " " + ii.second.fn->arguments[i].second;
				}
				out += ")/";
			}
		}
		Function* f = compilation->GetFunctionAtPoint(ffile, line);
		if (f && f->expression)
		{
			Scope* scope = f->expression->GetBlock()->scope;
			for (auto ii : scope->named_values)
				if (ii.second.type->type == Types::Pointer )
					out += ii.first + "L/"+ii.second.type->base->ToString() + " " + ii.first+"/";
				else
					out += ii.first + "L/" + ii.second.type->ToString() + " " + ii.first + "/";

			if (scope->prev)
			{
				for (auto ii : scope->prev->named_values)
					if (ii.second.type->type == Types::Pointer)
						out += ii.first + "L/" + ii.second.type->base->ToString() + " " + ii.first + "/";
					else
						out += ii.first + "L/" + ii.second.type->ToString() + " " + ii.first + "/";
			}
		}

		delete compilation;
		delete project;

		strcpy(data, out.c_str());
		return data;
	}
}


