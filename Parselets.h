
#ifndef _JET_PARSELETS_HEADER
#define _JET_PARSELETS_HEADER

#include "Expressions.h"
#include "ControlExpressions.h"
#include "DeclarationExpressions.h"
#include "Token.h"

#include <stdlib.h>

namespace Jet
{
	enum Precedence {
		// Ordered in increasing precedence.
		ASSIGNMENT = 1,
		LOGICAL = 2,// || or &&
		CONDITIONAL = 3,
		BINARY = 4,
		SUM = 5,
		PRODUCT = 6,
		PREFIX = 7,
		POSTFIX = 8,
		CALL = 9,//was 9 before
	};

	class Parser;
	class Expression;

	//Parselets
	class PrefixParselet
	{
	public:
		virtual ~PrefixParselet() {};
		virtual Expression* parse(Parser* parser, Token token) = 0;
	};

	class NameParselet: public PrefixParselet
	{
	public:
		Expression* parse(Parser* parser, Token token);
	};

	class NumberParselet: public PrefixParselet
	{
	public:
		Expression* parse(Parser* parser, Token token)
		{
			return new NumberExpression(token);
		}
	};

	class LambdaAndAttributeParselet : public PrefixParselet
	{
	public:
		Expression* parse(Parser* parser, Token token);
	};

	class StringParselet: public PrefixParselet
	{
	public:
		Expression* parse(Parser* parser, Token token)
		{
			return new StringExpression(token.text, token);
		}
	};

	class GroupParselet: public PrefixParselet
	{
	public:
		Expression* parse(Parser* parser, Token token);
	};

	class PrefixOperatorParselet: public PrefixParselet
	{
		int precedence;
	public:
		PrefixOperatorParselet(int precedence)
		{
			this->precedence = precedence;
		}

		Expression* parse(Parser* parser, Token token);

		int GetPrecedence()
		{
			return precedence;
		}
	};

	class CastParselet : public PrefixParselet
	{

	public:

		Expression* parse(Parser* parser, Token token);

		int GetPrecedence()
		{
			return Precedence::PREFIX;
		}
	};

	class NewParselet : public PrefixParselet
	{

	public:

		Expression* parse(Parser* parser, Token token);

		int GetPrecedence()
		{
			return Precedence::PREFIX;
		}
	};

	class FreeParselet : public PrefixParselet
	{

	public:

		Expression* parse(Parser* parser, Token token);

		int GetPrecedence()
		{
			return Precedence::PREFIX;
		}
	};

	class InfixParselet
	{
	public:
		virtual ~InfixParselet() {};
		virtual Expression* parse(Parser* parser, Expression* left, Token token) = 0;

		virtual int getPrecedence() = 0;
	};

	class AssignParselet: public InfixParselet
	{
	public:
		Expression* parse(Parser* parser, Expression* left, Token token);

		int getPrecedence()
		{
			return Precedence::ASSIGNMENT;
		}
	};

	class ScopeParselet : public InfixParselet
	{
	public:
		Expression* parse(Parser* parser, Expression* left, Token token);

		int getPrecedence()
		{
			return Precedence::ASSIGNMENT;
		}
	};

	class OperatorAssignParselet: public InfixParselet
	{
	public:

		Expression* parse(Parser* parser, Expression* left, Token token);

		int getPrecedence()
		{
			return Precedence::ASSIGNMENT;
		}
	};

	class PostfixOperatorParselet: public InfixParselet
	{
		int precedence;
	public:
		PostfixOperatorParselet(int precedence)
		{
			this->precedence = precedence;
		}

		Expression* parse(Parser* parser, Expression* left, Token token)
		{
			return new PostfixExpression(left, token);
		}

		int getPrecedence()
		{
			return precedence;
		}
	};

	class BinaryOperatorParselet: public InfixParselet
	{
		int precedence;
		bool isRight;
	public:
		BinaryOperatorParselet(int precedence, bool isRight)
		{
			this->precedence = precedence;
			this->isRight = isRight;
		}

		Expression* parse(Parser* parser, Expression* left, Token token);

		int getPrecedence()
		{
			return precedence;
		}
	};

	class IndexParselet: public InfixParselet
	{
	public:
		Expression* parse(Parser* parser, Expression* left, Token token);

		int getPrecedence()
		{
			//replace precedence here
			return 9;// 9;
		}
	};

	class MemberParselet: public InfixParselet
	{
	public:
		Expression* parse(Parser* parser, Expression* left, Token token);

		int getPrecedence()
		{
			//replace precedence here
			return 9;// 3;//maybe?
		}
	};

	class PointerMemberParselet : public InfixParselet
	{
	public:
		Expression* parse(Parser* parser, Expression* left, Token token);

		int getPrecedence()
		{
			//replace precedence here
			return 9;// 3;//maybe?
		}
	};

	class CallParselet: public InfixParselet
	{
	public:

		Expression* parse(Parser* parser, Expression* left, Token token);

		int getPrecedence()
		{
			return Precedence::CALL;//whatever postfix precedence is
		}
	};

	class StatementParselet
	{
	public:
		bool TrailingSemicolon;
		StatementParselet() { TrailingSemicolon = false;}
		virtual ~StatementParselet() {};
		virtual Expression* parse(Parser* parser, Token token) = 0;
	};

	class ReturnParselet: public StatementParselet
	{
	public:
		ReturnParselet()
		{
			this->TrailingSemicolon = true;
		}

		Expression* parse(Parser* parser, Token token);
	};

	class ContinueParselet: public StatementParselet
	{
	public:
		ContinueParselet()
		{
			this->TrailingSemicolon = true;
		}

		Expression* parse(Parser* parser, Token token)
		{
			return new ContinueExpression(token);
		}
	};

	class CaseParselet : public StatementParselet
	{
	public:
		CaseParselet()
		{
			this->TrailingSemicolon = false;
		}

		Expression* parse(Parser* parser, Token token);
	};

	class DefaultParselet : public StatementParselet
	{
	public:
		DefaultParselet()
		{
			this->TrailingSemicolon = false;
		}

		Expression* parse(Parser* parser, Token token);
	};

	class TraitParselet : public StatementParselet
	{
	public:
		TraitParselet()
		{
			this->TrailingSemicolon = false;
		}

		Expression* parse(Parser* parser, Token token);
	};

	class BreakParselet: public StatementParselet
	{
	public:
		BreakParselet()
		{
			this->TrailingSemicolon = true;
		}

		Expression* parse(Parser* parser, Token token)
		{
			return new BreakExpression(token);
		}
	};

	class SizeofParselet : public PrefixParselet
	{
	public:
		SizeofParselet()
		{
			
		}

		Expression* parse(Parser* parser, Token token);
	};

	class TypeofParselet : public PrefixParselet
	{
	public:
		TypeofParselet()
		{

		}

		Expression* parse(Parser* parser, Token token);
	};

	class WhileParselet: public StatementParselet
	{
	public:
		WhileParselet()
		{
			this->TrailingSemicolon = false;
		}

		Expression* parse(Parser* parser, Token token);
	};

	class FunctionParselet: public StatementParselet
	{
	public:
		FunctionParselet()
		{
			this->TrailingSemicolon = false;
		}

		Expression* parse(Parser* parser, Token token);
	};

	class TypedefParselet : public StatementParselet
	{
	public:
		TypedefParselet()
		{
			this->TrailingSemicolon = true;
		}

		Expression* parse(Parser* parser, Token token);
	};

	class StructParselet : public StatementParselet
	{
	public:
		StructParselet()
		{
			this->TrailingSemicolon = false;
		}

		Expression* parse(Parser* parser, Token token);
	};

	class EnumParselet : public StatementParselet
	{
	public:
		EnumParselet()
		{
			this->TrailingSemicolon = false;
		}

		Expression* parse(Parser* parser, Token token);
	};

	class ExternParselet : public StatementParselet
	{
	public:
		ExternParselet()
		{
			this->TrailingSemicolon = true;
		}

		Expression* parse(Parser* parser, Token token);
	};

	class IfParselet: public StatementParselet
	{
	public:
		IfParselet()
		{
			this->TrailingSemicolon = false;
		}

		Expression* parse(Parser* parser, Token token);
	};

	class ForParselet: public StatementParselet
	{
	public:
		ForParselet()
		{
			this->TrailingSemicolon = false;
		}

		Expression* parse(Parser* parser, Token token);
	};

	class SwitchParselet : public StatementParselet
	{
	public:
		SwitchParselet()
		{
			this->TrailingSemicolon = false;
		}

		Expression* parse(Parser* parser, Token token);
	};

	class MatchParselet : public StatementParselet
	{
	public:
		MatchParselet()
		{
			this->TrailingSemicolon = false;
		}

		Expression* parse(Parser* parser, Token token);
	};

	class UnionParselet : public StatementParselet
	{
	public:
		UnionParselet()
		{
			this->TrailingSemicolon = true;
		}

		Expression* parse(Parser* parser, Token token);
	};

	class LocalParselet: public StatementParselet
	{
	public:
		LocalParselet()
		{
			this->TrailingSemicolon = true;
		}

		Expression* parse(Parser* parser, Token token);
	};

	//use me for parallelism
	class ConstParselet: public StatementParselet
	{
	public:
		ConstParselet()
		{
			this->TrailingSemicolon = true;
		}

		Expression* parse(Parser* parser, Token token);
	};

	class NamespaceParselet : public StatementParselet
	{
	public:
		NamespaceParselet()
		{
			this->TrailingSemicolon = false;
		}

		Expression* parse(Parser* parser, Token token);
	};

	class YieldParselet: public StatementParselet
	{
	public:
		YieldParselet()
		{
			this->TrailingSemicolon = true;
		}
		Expression* parse(Parser* parser, Token token);
	};

	class InlineYieldParselet: public PrefixParselet
	{
	public:
		Expression* parse(Parser* parser, Token token);
	};
};

#endif