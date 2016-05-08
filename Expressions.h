#ifndef _EXPRESSIONS_HEADER
#define _EXPRESSIONS_HEADER

#include <string>
#include <stdio.h>
#include <vector>

#include "Compiler.h"
#include "CompilerContext.h"
#include "ExpressionVisitor.h"
#include "Source.h"
#include "Types\Function.h"


namespace Jet
{
	class Source;
	class Compiler;

	class Expression// : public SyntaxNode
	{
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

		virtual CValue Compile(CompilerContext* context) = 0;

		virtual void CompileDeclarations(CompilerContext* context) = 0;

		virtual void Print(std::string& output, Source* source) = 0;

		//add a type checking function
		//will be tricky to get implemented correctly will need somewhere to store variables and member types
		virtual Type* TypeCheck(CompilerContext* context) = 0;

		virtual void Visit(ExpressionVisitor* visitor) = 0;//visits all subexpressions
	};

	class IStorableExpression
	{
	public:
		virtual void CompileStore(CompilerContext* context, CValue right) = 0;
	};

	class NameExpression : public Expression, public IStorableExpression
	{

	public:
		Token token;
		std::vector<Token>* templates;

		NameExpression(Token name, std::vector<Token>* templates = 0)
		{
			this->token = name;
			this->templates = templates;
		}

		std::string GetName()
		{
			return token.text;
		}

		CValue Compile(CompilerContext* context);

		void CompileStore(CompilerContext* context, CValue right)
		{
			//need to do cast if necessary
			context->CurrentToken(&token);

			context->Store(token.text, right);
		}

		void CompileDeclarations(CompilerContext* context) {};

		virtual Type* TypeCheck(CompilerContext* context)
		{
			return context->TCGetVariable(token.text)->base;
			//lookup type
		}

		void Print(std::string& output, Source* source) 
		{ 
			token.Print(output, source); 
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);
		}
	};

	class ScopedExpression : public Expression
	{

	public:
		Token token;
		Expression* left, *right;

		ScopedExpression(Token tok, Expression* left, Expression* right) : left(left), right(right)
		{
			this->token = tok;
		}

		std::string GetName()
		{
			return token.text;
		}

		CValue Compile(CompilerContext* context)
		{
			auto v = dynamic_cast<NameExpression*>(left);
			if (v == 0)
				context->root->Error("Invalid Namespace", this->token);

			context->SetNamespace(v->GetName());

			right->Compile(context);

			context->PopNamespace();

			return CValue();
		}

		void CompileDeclarations(CompilerContext* context) {};

		void Print(std::string& output, Source* source)
		{
			left->Print(output, source);
			token.Print(output, source);
			right->Print(output, source);
		}

		virtual Type* TypeCheck(CompilerContext* context)
		{
			throw 7;
			return 0;
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);
		}
	};

	class NewExpression : public Expression
	{

	public:
		Token token, type;
		Token open_bracket, close_bracket;
		Expression* size;
		NewExpression(Token tok, Token type, Expression* size)
		{
			this->token = tok;
			this->type = type;
			this->size = size;
		}

		CValue Compile(CompilerContext* context);

		void CompileDeclarations(CompilerContext* context) {};

		virtual Type* TypeCheck(CompilerContext* context)
		{
			auto ty = context->root->LookupType(type.text);
			return ty->GetPointerType();
		}

		void Print(std::string& output, Source* source)
		{
			//left->Print(output, source);
			token.Print(output, source);
			type.Print(output, source);

			if (this->size)
			{
				open_bracket.Print(output, source);// output += '['; fix this
				size->Print(output, source);
				close_bracket.Print(output, source);// output += ']'; fix this
			}
			//right->Print(output, source);
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);
		}
	};

	/*class ArrayExpression: public Expression
	{
	std::vector<Expression*> initializers;
	public:
	ArrayExpression(std::vector<Expression*>&& inits) : initializers(inits)
	{

	}

	~ArrayExpression()
	{
	//if (this->initializers)
	//{
	for (auto ii: this->initializers)
	delete ii;
	//}
	//delete this->initializers;
	}

	virtual void SetParent(Expression* parent)
	{
	this->Parent = parent;
	//if (this->initializers)
	//{
	for (auto ii: this->initializers)
	ii->SetParent(this);
	//}
	}

	CValue Compile(CompilerContext* context);
	};*/

	/*class ObjectExpression: public Expression
	{
	std::vector<std::pair<std::string, Expression*>>* inits;
	public:
	ObjectExpression()
	{
	inits = 0;
	}

	ObjectExpression(std::vector<std::pair<std::string, Expression*>>* initializers) : inits(initializers)
	{
	}

	~ObjectExpression()
	{
	if (this->inits)
	for (auto ii: *this->inits)
	delete ii.second;
	delete this->inits;
	}

	virtual void SetParent(Expression* parent)
	{
	this->Parent = parent;
	if (this->inits)
	for (auto ii: *this->inits)
	ii.second->SetParent(this);
	}

	CValue Compile(CompilerContext* context);
	};*/

	class NumberExpression : public Expression
	{
		Token token;
	public:
		NumberExpression(Token token)
		{
			this->token = token;
		}

		int GetIntValue()
		{
			bool isint = true;
			bool ishex = false;
			for (int i = 0; i < this->token.text.length(); i++)
			{
				if (this->token.text[i] == '.')
					isint = false;
			}

			if (token.text.length() >= 3)
			{
				std::string substr = token.text.substr(2);
				if (token.text[1] == 'x')
				{
					unsigned long long num = std::stoull(substr, nullptr, 16);
					return num;
				}
				else if (token.text[1] == 'b')
				{
					unsigned long long num = std::stoull(substr, nullptr, 2);
					return num;
				}
			}

			//ok, lets get the type from what kind of constant it is
			//get type from the constant
			//this is pretty terrible, come back later
			if (isint)
				return std::stoi(this->token.text);
			else
				return ::atof(token.text.c_str());
		}

		double GetValue()
		{
			bool isint = true;
			bool ishex = false;
			for (int i = 0; i < this->token.text.length(); i++)
			{
				if (this->token.text[i] == '.')
					isint = false;
			}

			if (token.text.length() >= 3)
			{
				std::string substr = token.text.substr(2);
				if (token.text[1] == 'x')
				{
					unsigned long long num = std::stoull(substr, nullptr, 16);
					return num;
				}
				else if (token.text[1] == 'b')
				{
					unsigned long long num = std::stoull(substr, nullptr, 2);
					return num;
				}
			}

			//ok, lets get the type from what kind of constant it is
			//get type from the constant
			//this is pretty terrible, come back later
			if (isint)
				return std::stoi(this->token.text);
			else
				return ::atof(token.text.c_str());
		}

		CValue Compile(CompilerContext* context);

		void CompileDeclarations(CompilerContext* context) {};

		void Print(std::string& output, Source* source)
		{
			token.Print(output, source);
		}

		virtual Type* TypeCheck(CompilerContext* context)
		{
			//get the type fixme later
			return context->root->IntType;
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);
		}
	};

	/*class NullExpression: public Expression
	{
	public:
	NullExpression()
	{
	}

	void print()
	{
	printf("Null");
	}

	void Compile(CompilerContext* context);
	};*/

	class StringExpression : public Expression
	{
		Token token;
		std::string value;
	public:
		StringExpression(std::string value, Token token)
		{
			this->value = value;
			this->token = token;
		}

		std::string GetValue()
		{
			return this->value;
		}

		CValue Compile(CompilerContext* context);

		void CompileDeclarations(CompilerContext* context) {};

		void Print(std::string& output, Source* source)
		{
			auto code = token.text_ptr;
			auto trivia = token.text_ptr - token.trivia_length;
			for (int i = 0; i < token.trivia_length; i++)
				output += trivia[i];

			output += "\"";
			//fixme!
			auto cur = &source->GetLinePointer(token.line)[token.column]/* token.text_ptr*/ + 1;
			do
			{
				if (*cur == '\"' && *(cur - 1) != '\\')
					break;
				output += *cur;
			} while (*cur++ != 0);
			//token.Print(output, source);
			output += "\"";
		}

		virtual Type* TypeCheck(CompilerContext* context)
		{
			return context->root->LookupType("char*");
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);
		}
	};

	class IndexExpression : public Expression, public IStorableExpression
	{

	public:
		Token member;
		Expression* index;
		Expression* left;
		Token token, close_bracket;
		IndexExpression(Expression* left, Expression* index, Token t, Token cb)
		{
			this->token = t;
			this->left = left;
			this->index = index;
			this->close_bracket = cb;
		}

		IndexExpression(Expression* left, Token index, Token t)
		{
			this->token = t;
			this->left = left;
			this->index = 0;
			this->member = index;
		}

		~IndexExpression()
		{
			delete left;
			delete index;
		}

		CValue Compile(CompilerContext* context);

		CValue GetElementPointer(CompilerContext* context);
		CValue GetBaseElementPointer(CompilerContext* context);

		Type* GetType(CompilerContext* context, bool tc = false);
		Type* GetBaseType(CompilerContext* context, bool tc = false);
		Type* GetBaseType(Compilation* compiler);

		void CompileStore(CompilerContext* context, CValue right);

		void CompileDeclarations(CompilerContext* context) {};

		void Print(std::string& output, Source* source)
		{
			this->left->Print(output, source);
			token.Print(output, source);
			if (token.type == TokenType::Dot || token.type == TokenType::Pointy)
				member.Print(output, source);
			else if (token.type == TokenType::LeftBracket)
			{
				index->Print(output, source);
				close_bracket.Print(output, source);// output += ']'; fix me!
			}
		}

		virtual Type* TypeCheck(CompilerContext* context)
		{
			//auto loc = this->GetElementPointer(context);
			auto type = this->GetType(context, true);
			if (type->type == Types::Function)
				return type;
			return type;// ->base;
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);

			if (index)
				index->Visit(visitor);
			if (left)
				left->Visit(visitor);
		}
	};

	class AssignExpression : public Expression
	{
		Token token;
		Expression* left;
		Expression* right;
	public:
		AssignExpression(Token token, Expression* l, Expression* r)
		{
			this->token = token;
			this->left = l;
			this->right = r;
		}

		~AssignExpression()
		{
			delete this->right;
			delete this->left;
		}

		virtual void SetParent(Expression* parent)
		{
			this->parent = parent;
			right->SetParent(this);
			left->SetParent(this);
		}

		CValue Compile(CompilerContext* context);

		void CompileDeclarations(CompilerContext* context) {};

		void Print(std::string& output, Source* source)
		{
			left->Print(output, source);
			token.Print(output, source);
			right->Print(output, source);
		}

		virtual Type* TypeCheck(CompilerContext* context)
		{
			auto tl = left->TypeCheck(context);
			auto tr = right->TypeCheck(context);
			//return the type of the left
			//check if can be assigned
			if (tl == tr)
				return tl;

			//todo: make this error
			//throw 7;
			return tr;
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);

			left->Visit(visitor);
			right->Visit(visitor);
		}
	};

	class OperatorAssignExpression : public Expression
	{
		Token token;

		Expression* left;
		Expression* right;
	public:
		OperatorAssignExpression(Token token, Expression* l, Expression* r)
		{
			this->token = token;
			this->left = l;
			this->right = r;
		}
		//come up with classes/traits/inheritance stuff
		~OperatorAssignExpression()
		{
			delete this->right;
			delete this->left;
		}

		virtual void SetParent(Expression* parent)
		{
			this->parent = parent;
			right->SetParent(this);
			left->SetParent(this);
		}

		CValue Compile(CompilerContext* context);

		virtual Type* TypeCheck(CompilerContext* context)
		{
			throw 7;
			//check it
			return 0;
		}

		void Print(std::string& output, Source* source)
		{
			left->Print(output, source);
			token.Print(output, source);
			right->Print(output, source);
		}

		void CompileDeclarations(CompilerContext* context) {};

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);
			right->Visit(visitor);
			left->Visit(visitor);
		}
	};

	/*class SwapExpression: public Expression
	{
	Expression* left;
	Expression* right;
	public:
	SwapExpression(Expression* l, Expression* r)
	{
	this->left = l;
	this->right = r;
	}

	~SwapExpression()
	{
	delete this->right;
	delete this->left;
	}

	void SetParent(Expression* parent)
	{
	this->Parent = parent;
	right->SetParent(this);
	left->SetParent(this);
	}

	void Compile(CompilerContext* context);
	};*/

	class CastExpression : public Expression//, public IStorableExpression
	{
		Token begin, end;
		Token type;
		Expression* right;
	public:
		CastExpression(Token type, Token begin, Expression* r, Token end)
		{
			this->type = type;
			this->begin = begin;
			this->right = r;
			this->end = end;
		}

		~CastExpression()
		{
			delete this->right;
		}

		void SetParent(Expression* parent)
		{
			this->parent = parent;
			right->SetParent(this);
		}

		CValue Compile(CompilerContext* context)
		{
			auto t = context->root->LookupType(type.text);

			return context->DoCast(t, right->Compile(context), true);
		}

		virtual Type* TypeCheck(CompilerContext* context)
		{
			//throw 7;
			//check if the cast can be made
			//todo
			return context->root->LookupType(this->type.text);
			//return 0;
		}

		void CompileDeclarations(CompilerContext* context) {};

		void Print(std::string& output, Source* source)
		{
			begin.Print(output, source);
			type.Print(output, source);
			end.Print(output, source);
			right->Print(output, source);
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);
			right->Visit(visitor);
		}
	};

	class PrefixExpression : public Expression, public IStorableExpression
	{
		Token _operator;

		Expression* right;
	public:
		PrefixExpression(Token type, Expression* r)
		{
			this->_operator = type;
			this->right = r;
		}

		~PrefixExpression()
		{
			delete this->right;
		}

		void SetParent(Expression* parent)
		{
			this->parent = parent;
			right->SetParent(this);
		}

		virtual Type* TypeCheck(CompilerContext* context);

		void CompileStore(CompilerContext* context, CValue right)
		{
			if (_operator.type != TokenType::Asterisk)
				context->root->Error("Cannot store into this expression!", _operator);

			if (this->_operator.type == TokenType::Asterisk)
			{
				auto loc = this->right->Compile(context);

				right = context->DoCast(loc.type->base, right);

				context->root->builder.CreateStore(right.val, loc.val);
				return;
			}
			context->root->Error("Unimplemented!", _operator);
		}

		CValue Compile(CompilerContext* context);

		void CompileDeclarations(CompilerContext* context) {};

		void Print(std::string& output, Source* source)
		{
			_operator.Print(output, source);
			right->Print(output, source);
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);
			right->Visit(visitor);
		}
	};

	class PostfixExpression : public Expression
	{
		Token _operator;

		Expression* left;
	public:
		PostfixExpression(Expression* l, Token type)
		{
			this->_operator = type;
			this->left = l;
		}

		~PostfixExpression()
		{
			delete this->left;
		}

		void SetParent(Expression* parent)
		{
			this->parent = parent;
			left->SetParent(this);
		}

		virtual Type* TypeCheck(CompilerContext* context)
		{
			auto left = this->left->TypeCheck(context);
			//throw 7;
			return 0;
		}

		CValue Compile(CompilerContext* context);

		void CompileDeclarations(CompilerContext* context) {};

		void Print(std::string& output, Source* source)
		{
			left->Print(output, source);
			_operator.Print(output, source);
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);
			left->Visit(visitor);
		}
	};

	class OperatorExpression : public Expression
	{
		Token _operator;

		Expression* left, *right;

	public:
		OperatorExpression(Expression* l, Token type, Expression* r)
		{
			this->_operator = type;
			this->left = l;
			this->right = r;
		}

		~OperatorExpression()
		{
			delete this->right;
			delete this->left;
		}

		void SetParent(Expression* parent)
		{
			this->parent = parent;
			left->SetParent(this);
			if (right)
				right->SetParent(this);
		}

		virtual Type* TypeCheck(CompilerContext* context)
		{
			auto left = this->left->TypeCheck(context);
			auto right = this->right->TypeCheck(context);

			//ok, lets return the correct type

			switch (this->_operator.type)
			{
			case TokenType::LessThan:
			case TokenType::LessThanEqual:
			case TokenType::GreaterThan:
			case TokenType::GreaterThanEqual:
			case TokenType::Equals:
			case TokenType::NotEqual:
				return context->root->BoolType;
			default:
				return left;
			}
			//check if operation is ok
			//throw 7;
			return left;
		}

		CValue Compile(CompilerContext* context);

		void CompileDeclarations(CompilerContext* context) {};

		void Print(std::string& output, Source* source)
		{
			left->Print(output, source);
			_operator.Print(output, source);
			right->Print(output, source);
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);
			left->Visit(visitor);
			right->Visit(visitor);
		}
	};

	class StatementExpression : public Expression
	{
	public:
	};

	class BlockExpression : public Expression
	{
		friend class ScopeExpression;
		bool no_brackets;

	public:
		Token start, end, eof;

		std::vector<Expression*> statements;
		BlockExpression(Token start_bracket, Token end_bracket, std::vector<Expression*>&& statements) : statements(statements)
		{
			this->no_brackets = false;
			this->start = start_bracket;
			this->end = end_bracket;
		}

		BlockExpression(std::vector<Expression*>&& statements) : statements(statements)
		{
			this->no_brackets = true;
		}

		~BlockExpression()
		{
			for (auto ii : this->statements)
				delete ii;
		}

		BlockExpression() { };

		void SetParent(Expression* parent)
		{
			this->parent = parent;
			for (auto ii : statements)
				ii->SetParent(this);
		}

		CValue Compile(CompilerContext* context)
		{
			for (auto ii : statements)
			{
				try
				{
					ii->Compile(context);
				}
				catch (...)
				{

				}
			}

			return CValue();
		}

		void CompileDeclarations(CompilerContext* context)
		{
			for (auto ii : statements)
			{
				try
				{
					ii->CompileDeclarations(context);
				}
				catch (...)
				{

				}
			}
		};

		virtual Type* TypeCheck(CompilerContext* context)
		{
			for (auto ii : this->statements)
				ii->TypeCheck(context);
			return 0;
		}

		void Print(std::string& output, Source* source)
		{
			if (!no_brackets)
				this->start.Print(output, source);
			for (auto ii : statements)
			{
				ii->Print(output, source);
				if (ii->semicolon.text.length())
					ii->semicolon.Print(output, source);
			}
			if (!no_brackets)
				this->end.Print(output, source);

			if (this->eof.trivia_length)
				this->eof.Print(output, source);
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);

			for (auto ii : this->statements)
				ii->Visit(visitor);
		}
	};

	class ScopeExpression : public BlockExpression
	{

	public:
		Scope* scope;//debug shizzle

		ScopeExpression(BlockExpression* r)
		{
			this->no_brackets = r->no_brackets;
			this->start = r->start;
			this->end = r->end;
			this->statements = r->statements;
			r->statements.clear();
			delete r;

			this->scope = 0;
		}

		virtual Type* TypeCheck(CompilerContext* context)
		{
			context->TCPushScope();
			BlockExpression::TypeCheck(context);
			context->TCPopScope();
			return 0;
		}

		CValue Compile(CompilerContext* context)
		{
			//push scope
			this->scope = context->PushScope();

			CValue tmp = BlockExpression::Compile(context);

			//pop scope
			context->PopScope();

			return tmp;
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);

			for (auto ii : this->statements)
				ii->Visit(visitor);
		}
	};

	/*class ForEachExpression: public Expression
	{
	Token name;
	Expression* container;
	ScopeExpression* block;
	public:
	ForEachExpression(Token name, Expression* container, ScopeExpression* block)
	{
	this->container = container;
	this->block = block;
	this->name = name;
	}

	~ForEachExpression()
	{
	delete this->block;
	delete this->container;
	}

	void SetParent(Expression* parent)
	{
	this->Parent = parent;
	block->SetParent(this);
	}

	void Compile(CompilerContext* context)
	{
	context->PushScope();

	auto uuid = context->GetUUID();
	context->RegisterLocal(this->name.text);
	context->RegisterLocal("_iter");

	//context->Load(this->container.text);
	this->container->Compile(context);
	context->Duplicate();
	context->LoadIndex("iterator");
	context->ECall(1);
	//context->Duplicate();
	context->Store("_iter");
	//context->JumpFalse(("_foreachend"+uuid).c_str());

	context->Label("_foreachstart"+uuid);

	context->Load("_iter");
	context->Duplicate();
	context->LoadIndex("advance");
	context->ECall(1);
	context->JumpFalse(("_foreachend"+uuid).c_str());

	context->Load("_iter");
	context->Duplicate();
	context->LoadIndex("current");
	context->ECall(1);
	context->Store(this->name.text);

	//context->ForEach(this->name.text, "_foreachstart"+uuid, "_foreachend"+uuid);
	//finish implementing foreach instructions
	context->PushLoop("_foreachend"+uuid, "_foreachstart"+uuid);
	this->block->Compile(context);
	context->PopLoop();

	//context->PostForEach(



	context->Jump(("_foreachstart"+uuid).c_str());
	context->Label("_foreachend"+uuid);

	context->PopScope();
	}
	};*/

	class SizeofExpression : public Expression
	{
		Token begin, end;
		Token type;
		Token token;
	public:
		SizeofExpression(Token token, Token begin, Token type, Token end)
		{
			this->type = type;
			this->begin = begin;
			this->token = token;
			this->end = end;
		}

		void SetParent(Expression* parent)
		{
			this->parent = parent;
		}

		CValue Compile(CompilerContext* context);

		void CompileDeclarations(CompilerContext* context) {};

		void Print(std::string& output, Source* source)
		{
			token.Print(output, source);
			begin.Print(output, source);
			type.Print(output, source);
			end.Print(output, source);
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);
		}

		virtual Type* TypeCheck(CompilerContext* context)
		{
			return 0;
		}
	};

	class GroupExpression : public Expression
	{
		Token begin, end;
		Expression* expr;
	public:
		GroupExpression(Token begin, Expression* expr, Token end)
		{
			this->begin = begin;
			this->expr = expr;
			this->end = end;
		}

		void SetParent(Expression* parent)
		{
			this->parent = parent;
			this->expr->parent = this;
		}

		CValue Compile(CompilerContext* context)
		{
			return this->expr->Compile(context);
		}

		void CompileDeclarations(CompilerContext* context) { this->expr->Compile(context); };

		void Print(std::string& output, Source* source)
		{
			begin.Print(output, source);
			expr->Print(output, source);
			end.Print(output, source);
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);
		}

		virtual Type* TypeCheck(CompilerContext* context)
		{
			return this->expr->TypeCheck(context);
		}
	};
}
#endif