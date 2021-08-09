#ifndef _EXPRESSION_HEADER
#define _EXPRESSION_HEADER

#include <string>
#include <stdio.h>
#include <vector>

#include <Token.h>

#include <types/Types.h>

namespace Jet
{
	class Source;
	class Compiler;
    class CompilerContext;
    class ExpressionVisitor;

	class Expression
	{
		std::string qualified_namespace_;// cache
	public:
		Token semicolon;
		Expression()
		{
			parent = 0;
		}

		virtual ~Expression()
		{

		}

		Expression* parent;
		virtual void SetParent(Expression* parent)
		{
			this->parent = parent;
		}

		inline const std::string& GetNamespaceQualifier()
        {
	        if (qualified_namespace_.length())
	        {
    		    return qualified_namespace_;
    	    }

    	    // go down the tree and build up a qualified namespace
    	    if (!this->parent)
        	{
    	    	return qualified_namespace_;//no parent, no problem
        	}

    	    qualified_namespace_ = this->parent->GetNamespaceQualifier();

        	const char* ns = GetNamespace();
    	    if (ns)
        	{
    	    	qualified_namespace_ += "__";
        		qualified_namespace_ += ns;
    	    }
	
        	return qualified_namespace_;
        }

        inline void ResetNamespace() { qualified_namespace_.clear(); }

		virtual CValue Compile(CompilerContext* context) = 0;

		virtual void CompileDeclarations(CompilerContext* context) = 0;

		virtual void Print(std::string& output, Source* source) = 0;

		//add a type checking function
		//will be tricky to get implemented correctly will need somewhere to store variables and member types
		virtual Type* TypeCheck(CompilerContext* context) = 0;

		virtual void Visit(ExpressionVisitor* visitor) = 0;//visits all subexpressions

		virtual const char* GetNamespace() { return 0; }// returns the namespace that we create, if any
	};

	class IStorableExpression
	{
	public:
		virtual void CompileStore(CompilerContext* context, CValue right) = 0;
	};
}
#endif
