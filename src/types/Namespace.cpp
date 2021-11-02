#include "Types.h"
#include "../Compiler.h"
#include "../CompilerContext.h"
#include <expressions/Expressions.h>
#include <expressions/DeclarationExpressions.h>
#include "../Lexer.h"
#include "Function.h"

using namespace Jet;

Namespace::~Namespace()
{
	for (auto ii : this->members)
	{
		if (ii.second.type == SymbolType::Namespace)
			delete ii.second.ns;
	}
}

const std::string& Namespace::GetQualifiedName()
{
	if (qualified_name_.length())
	{
		return qualified_name_;
	}

	if (this->parent)
	{
		qualified_name_ = this->parent->GetQualifiedName();
		if (qualified_name_.length())
			qualified_name_ += "::";
		qualified_name_ += name;
	}
	else
	{
		qualified_name_ = name;
	}
	return qualified_name_;
}

//todo, condense the data later using numeric identifiers and a list rather than whole name on each also could sort by file
void add_location(Token token, std::string& data, Compilation* compilation)
{
	auto location = token.GetSource(compilation);

	data += "\n//!@!";
	data += location->filename;
	data += "@";
	data += std::to_string(token.line);
	data += '\n';
}

void Namespace::OutputMetadata(std::string& data, Compilation* compilation, bool globals)
{
    if (globals)
    {
        // output globals last
	    for (auto ii : this->members)
	    {
		    //ok, lets add debug location info
            if (ii.second.type == SymbolType::Variable)
            {
                add_location(ii.second.token, data, compilation);

                // add namespacing
                std::string ns = GetQualifiedName();
                if (ns.length())
                {
                    data += "namespace " + ns + " { ";
                }

                data += "extern " + ii.second.val->type->ToString() + " " + ii.first + ";\n";

                // close namespace
                if (ns.length())
                {
                    data += "}\n";
                }
            }
		    else if (ii.second.type == SymbolType::Namespace)
		    {
			    ii.second.ns->OutputMetadata(data, compilation, true);
		    }
        }
        return;
    }

	//ok, change this to output in blocks, give size and location/namespace
	for (auto ii : this->members)
	{
		//ok, lets add debug location info
        if (ii.second.type == SymbolType::Function && ii.second.fn->do_export)
		{
			if (ii.second.fn->expression)
				add_location(ii.second.fn->expression->token, data, compilation);

            if (ii.second.fn->is_c_function)
			    data += "extern_c fun " + ii.second.fn->return_type->ToString() + " ";
            else
			    data += "extern fun " + ii.second.fn->return_type->ToString() + " ";
			data += ii.first + "(";
			bool first = false;
			for (auto arg : ii.second.fn->arguments)
			{
				if (first)
					data += ",";
				else
					first = true;

				data += arg.first->ToString() + " " + arg.second;
			}
			data += ");";
		}
		else if (ii.second.type == SymbolType::Type)
		{
			if (ii.second.ty->type == Types::Struct)
			{
				if (ii.second.ty->data->template_base)
					continue;//dont bother exporting instantiated templates for now

				//set the current location
				if (ii.second.ty->data->expression)
					add_location(ii.second.ty->data->expression->token, data, compilation);

                // add namespacing
                auto index = ii.second.ty->data->name.find_last_of(':');
                std::string struct_name;
                if (index == std::string::npos)
                {
                    struct_name = ii.second.ty->data->name;
                }
                else
                {
                    struct_name = ii.second.ty->data->name.substr(index + 1);
                    std::string ns = ii.second.ty->data->name.substr(0, index - 1);
                    data += "namespace " + ns + " { ";
                }

				//export me
				if (ii.second.ty->data->templates.size() > 0)
				{
					data += "struct " + struct_name + "<";
					for (unsigned int i = 0; i < ii.second.ty->data->templates.size(); i++)
					{
						data += ii.second.ty->data->templates[i].first->name + " ";
						data += ii.second.ty->data->templates[i].second;
						if (i < ii.second.ty->data->templates.size() - 1)
							data += ',';
					}
					data += ">{";
				}
				else
				{
					data += "struct " + struct_name + "{";
				}
				for (auto var : ii.second.ty->data->struct_members)
				{
					if (var.type == 0 || var.type->type == Types::Invalid)//its a template probably?
					{
						data += var.type_name + " ";
						data += var.name + ";";
					}
					else if (var.type->type == Types::Array)
					{
						data += var.type->base->ToString() + " ";
						data += var.name + "[" + std::to_string(var.type->size) + "];";
					}
					else
					{
						data += var.type->ToString() + " ";
						data += var.name + ";";
					}
				}

				if (ii.second.ty->data->templates.size() > 0 && ii.second.ty->data->template_base == 0)
				{
					//output member functions somehow?
					auto expr = ii.second.ty->data->expression;
					for (auto ii : expr->members)
					{
						if (ii.type == ii.FunctionMember)
						{
							std::string source;
							auto src = ii.function->GetBlock()->start.GetSource(compilation);
							int line = ii.function->token.line;
							//subtract out any lines in the trivia
							for (int i = 1; i <= ii.function->token.trivia_length; i++)
							{
								if (ii.function->token.text_ptr[-i] == '\n')
									line -= 1;
							}
							data += "\n//!@!" + src->filename + "@" + std::to_string(line) + "\n";
							ii.function->Print(source, src);
							data += source;
						}
					}
					data += "}";
					continue;
				}
				data += "}";

                // close the namespace
                if (index != std::string::npos)
                {
                    data += " }";
                }

				//output member functions
				for (auto fun : ii.second.ty->data->functions)
				{
					if (fun.second->expression)
						add_location(fun.second->expression->token, data, compilation);

					data += "extern fun " + fun.second->return_type->ToString() + " " + ii.second.ty->data->name + "::";

					if (IsLetter(fun.first[0]) == false)
						if (!(fun.first[0] == '~' && fun.first.length() > 1 && IsLetter(fun.first[1])))
							data += "operator";

					data += fun.first + "(";
					bool first = false;
					for (unsigned int i = 1; i < fun.second->arguments.size(); i++)
					{
						if (first)
							data += ",";
						else
							first = true;

						data += fun.second->arguments[i].first->ToString() + " " + fun.second->arguments[i].second;
					}
					data += ");";
				}
			}
			else if (ii.second.ty->type == Types::Trait)
			{
				//if (ii.second.ty->data->expression)
				//	add_location(ii.second.ty->trait->expression->token, data, compilation);

				data += "trait " + ii.second.ty->trait->name;
				for (unsigned int i = 0; i < ii.second.ty->trait->templates.size(); i++)
				{
					if (i == 0)
						data += '<';

					data += ii.second.ty->trait->templates[i].second;

					if (i == ii.second.ty->trait->templates.size() - 1)
						data += '>';
					else
						data += ',';
				}
				data += "{";
				for (auto fun : ii.second.ty->trait->functions)
				{
					data += " fun " + fun.second->return_type->name + " " + fun.first + "(";
					bool first = false;
					for (auto arg : fun.second->arguments)
					{
						if (first)
							data += ",";
						else
							first = true;

						data += arg.first->ToString() + " " + arg.second;
					}
					data += ");";
				}
				data += "}";

				//export extension methods
				for (auto fun : ii.second.ty->trait->extension_methods)
				{
					auto source = fun.second->expression->GetBlock()->start.GetSource(compilation);

					if (fun.second->expression)
						add_location(fun.second->expression->token, data, compilation);
					data += "fun ";
					fun.second->expression->ret_type.Print(data, source);
					data += " ";
					data += ii.second.ty->trait->name + "::";
					data += fun.first + "(";
					int i = 0;
					for (auto ii : *fun.second->expression->args)
					{
						data += ii.type.text + " " + ii.name.text;
						if (i != fun.second->expression->args->size() - 1)
							data += ", ";
						//ii->Print(output, source);
						i++;
					}
					data += ")";

					fun.second->expression->GetBlock()->Print(data, source);
				}
			}
		}
		else if (ii.second.type == SymbolType::Namespace)
		{
			ii.second.ns->OutputMetadata(data, compilation, false);
		}
	}
}
