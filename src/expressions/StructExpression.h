#pragma once

#include <Token.h>

#include <expressions/Expressions.h>

namespace Jet
{
	class Source;
	class Compiler;
    class StructExpression;
    class FunctionExpression;
    class StructVariable;

    struct StructVariable
	{
		Token type, name, semicolon;
	};
	struct StructMember
	{
		enum MemberType
		{
			FunctionMember,
			VariableMember,
			DefinitionMember,
		};
		MemberType type;
		StructExpression* definition;
		FunctionExpression* function;
		StructVariable variable;
	};
	struct StructTemplate
	{
		Token type, name, comma;
	};
	class StructExpression : public Expression
	{
		friend class Compiler;
		friend class Type;
	public:
		Token token;
	private:
		Token name;


		Token start;
		Token end;

		Token colon;
		Token base_type;

		void AddConstructorDeclarations(Type* str, CompilerContext* context);
		void AddConstructors(CompilerContext* context);
	public:

		std::vector<StructTemplate>* templates;
		std::vector<StructMember> members;

		Token template_open, template_close;

		StructExpression(Token token, Token name, Token start, Token end, Token ob, std::vector<StructMember>&& members, std::vector<StructTemplate>* templates, Token cb, Token colon, Token base_type)
		{
			this->templates = templates;
			this->members = members;
			this->base_type = base_type;
			this->name = name;
			this->token = token;
			this->start = start;
			this->template_open = ob;
			this->template_close = cb;
			this->end = end;
			this->colon = colon;
		}

		~StructExpression();

		std::string GetName()
		{
			return this->name.text;
		}

		virtual Type* TypeCheck(CompilerContext* context);

		void SetParent(Expression* parent);

		CValue Compile(CompilerContext* context);

		void CompileDeclarations(CompilerContext* context);

		void Print(std::string& output, Source* source);

		virtual void Visit(ExpressionVisitor* visitor);

		// returns the namespace that we create, if any
		std::string namespaceprefix_;
		virtual const char* GetNamespace()
		{
			namespaceprefix_ += this->name.text;
			return namespaceprefix_.c_str();
		}
	};
}
