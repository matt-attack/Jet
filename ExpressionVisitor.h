#ifndef EXPRESSION_VISITOR_HEADER
#define EXPRESSION_VISITOR_HEADER

namespace Jet
{
	class NameExpression;
	class TypeofExpression;
	class LetExpression;
	class NumberExpression;
	class StringExpression;
	class GroupExpression;
	class IndexExpression;
	class AssignExpression;
	class OperatorAssignExpression;
	class PrefixExpression;
	class PostfixExpression;
	class OperatorExpression;
	class ScopeExpression;
	class BlockExpression;
	class WhileExpression;
	class ForExpression;
	class SwitchExpression;
	class CaseExpression;
	class DefaultExpression;
	class IfExpression;
	class CallExpression;
	class FunctionExpression;
	class ExternExpression;
	class TraitExpression;
	class StructExpression;
	class ReturnExpression;
	class BreakExpression;
	class ContinueExpression;
	class CastExpression;
	class SizeofExpression;
	class TypedefExpression;
	class NamespaceExpression;
	class NewExpression;
	class ScopedExpression;
	class AttributeExpression;
	class UnionExpression;
	class FreeExpression;
	class EnumExpression;
	class ExpressionVisitor
	{
	public:

		virtual void Visit(NameExpression* exp) {};

		virtual void Visit(LetExpression* exp) {};

		virtual void Visit(NumberExpression* exp) {};
		virtual void Visit(StringExpression* exp) {};

		virtual void Visit(IndexExpression* exp) {};

		virtual void Visit(AssignExpression* exp) {};
		virtual void Visit(OperatorAssignExpression* exp) {};

		virtual void Visit(PrefixExpression* exp) {};
		virtual void Visit(PostfixExpression* exp) {};

		virtual void Visit(OperatorExpression* exp) {};

		//idk what to do about these really
		virtual void Visit(BlockExpression* exp) {};
		virtual void Visit(ScopeExpression* exp) {};

		virtual void Visit(WhileExpression* exp) {};
		virtual void Visit(ForExpression* exp) {};

		virtual void Visit(SwitchExpression* exp) {};
		virtual void Visit(CaseExpression* exp) {};
		virtual void Visit(DefaultExpression* exp) {};

		virtual void Visit(IfExpression* exp) {};

		virtual void Visit(CallExpression* exp) {};

		virtual void Visit(FunctionExpression* exp) {};
		virtual void Visit(ExternExpression* exp) {};

		virtual void Visit(TraitExpression* exp) {};
		virtual void Visit(StructExpression* exp) {};

		virtual void Visit(ReturnExpression* exp) {};

		virtual void Visit(BreakExpression* exp) {};
		virtual void Visit(ContinueExpression* exp) {};

		virtual void Visit(CastExpression* exp) {};

		virtual void Visit(SizeofExpression* exp) {};
		virtual void Visit(TypedefExpression* exp) {};
		virtual void Visit(NewExpression* exp) {};
		virtual void Visit(FreeExpression* exp) {};

		virtual void Visit(NamespaceExpression* exp) {};
		virtual void Visit(ScopedExpression* exp) {};

		virtual void Visit(AttributeExpression* exp) {};

		virtual void Visit(UnionExpression* exp) {};
		virtual void Visit(EnumExpression* exp) {};

		virtual void Visit(GroupExpression* exp) {};

		virtual void Visit(TypeofExpression* exp) {};
	};
}

#endif