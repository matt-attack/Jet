#ifndef _EXPRESSIONS_HEADER
#define _EXPRESSIONS_HEADER

#include <string>
#include <stdio.h>
#include <vector>

#include "Compiler.h"
#include "CompilerContext.h"
#include "ExpressionVisitor.h"
#include "Source.h"

namespace Jet
{
	class Source;
	class Compiler;

	//typedef std::function<void(Expression*)> ExpressionVisitor;


	class Expression
	{
	public:
		Token semicolon;
		Expression()
		{
			Parent = 0;
		}

		virtual ~Expression()
		{

		}

		Expression* Parent;
		virtual void SetParent(Expression* parent)
		{
			this->Parent = parent;
		}

		virtual CValue Compile(CompilerContext* context) = 0;

		virtual void CompileDeclarations(CompilerContext* context) = 0;

		virtual void Print(std::string& output, Source* source) = 0;

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

		void Print(std::string& output, Source* source) { token.Print(output, source); }

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

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);
		}
	};

	class NewExpression : public Expression
	{

	public:
		Token token, type;
		Expression* size;
		NewExpression(Token tok, Token type, Expression* size)
		{
			this->token = tok;
			this->type = type;
			this->size = size;
		}

		CValue Compile(CompilerContext* context);

		void CompileDeclarations(CompilerContext* context) {};

		void Print(std::string& output, Source* source)
		{
			//left->Print(output, source);
			token.Print(output, source);
			type.Print(output, source);

			if (this->size)
			{
				output += '[';
				size->Print(output, source);
				output += ']';
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

	class LocalExpression : public Expression
	{
		Token token;
		std::vector<std::pair<Token, Token>>* _names;
		std::vector<Expression*>* _right;
	public:
		LocalExpression(Token token, std::vector<std::pair<Token, Token>>* names, std::vector<Expression*>* right)
		{
			this->token = token;
			this->_names = names;
			this->_right = right;
		}

		~LocalExpression()
		{
			if (this->_right)
				for (auto ii : *this->_right)
					delete ii;

			delete this->_right;
			delete this->_names;
		}

		virtual void SetParent(Expression* parent)
		{
			this->Parent = parent;
			if (this->_right)
				for (auto ii : *_right)
					ii->SetParent(this);
		}

		CValue Compile(CompilerContext* context);

		void CompileDeclarations(CompilerContext* context) {};

		void Print(std::string& output, Source* source)
		{
			this->token.Print(output, source);

			int i = 0;
			for (auto ii : *_names)
			{
				//output += " " + ii.first;
				if (ii.first.text.length() > 0)
					ii.first.Print(output, source);
				ii.second.Print(output, source);

				if (_right && i < _right->size())
				{
					output += " =";
					(*_right)[i++]->Print(output, source);
				}
			}
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);

			if (this->_right)
			{
				for (auto ii : *this->_right)
				{
					ii->Visit(visitor);
				}
			}
		}
	};

	class NumberExpression : public Expression
	{
		double value;
		Token token;
	public:
		NumberExpression(double value, Token token)
		{
			this->value = value;
			this->token = token;
		}

		double GetValue()
		{
			return this->value;
		}

		CValue Compile(CompilerContext* context);

		void CompileDeclarations(CompilerContext* context) {};

		void Print(std::string& output, Source* source)
		{
			token.Print(output, source);
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
		Token token;
		IndexExpression(Expression* left, Expression* index, Token t)
		{
			this->token = t;
			this->left = left;
			this->index = index;
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

		Type* GetType(CompilerContext* context);
		Type* GetBaseType(CompilerContext* context);
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
				output += ']';
			}
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
			this->Parent = parent;
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

		void SetParent(Expression* parent)
		{
			this->Parent = parent;
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
			this->Parent = parent;
			right->SetParent(this);
		}

		CValue Compile(CompilerContext* context)
		{
			auto t = context->root->LookupType(type.text);

			return context->DoCast(t, right->Compile(context), true);
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
			this->Parent = parent;
			right->SetParent(this);
		}

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
			this->Parent = parent;
			left->SetParent(this);
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
			this->Parent = parent;
			left->SetParent(this);
			right->SetParent(this);
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
		Token start, end;

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
			this->Parent = parent;
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

		void Print(std::string& output, Source* source)
		{
			//fix these { later
			if (!no_brackets)
				this->start.Print(output, source);// output += "{";
			for (auto ii : statements)
			{
				ii->Print(output, source);
				if (ii->semicolon.text.length())
					ii->semicolon.Print(output, source);
			}
			if (!no_brackets)
				this->end.Print(output, source);// output += "}";
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

	class WhileExpression : public Expression
	{
		Expression* condition;
		ScopeExpression* block;
		Token token;
	public:

		WhileExpression(Token token, Expression* cond, ScopeExpression* block)
		{
			this->condition = cond;
			this->block = block;
			this->token = token;
		}

		~WhileExpression()
		{
			delete condition;
			delete block;
		}

		virtual void SetParent(Expression* parent)
		{
			this->Parent = parent;
			block->SetParent(this);
			condition->SetParent(this);
		}

		CValue Compile(CompilerContext* context)
		{
			context->CurrentToken(&token);

			llvm::BasicBlock *start = llvm::BasicBlock::Create(llvm::getGlobalContext(), "whilestart");
			llvm::BasicBlock *body = llvm::BasicBlock::Create(llvm::getGlobalContext(), "whilebody");
			llvm::BasicBlock *end = llvm::BasicBlock::Create(llvm::getGlobalContext(), "whileend");


			context->root->builder.CreateBr(start);
			context->f->getBasicBlockList().push_back(start);
			context->root->builder.SetInsertPoint(start);

			auto cond = this->condition->Compile(context);
			cond = context->DoCast(context->root->BoolType, cond);
			context->root->builder.CreateCondBr(cond.val, body, end);


			context->f->getBasicBlockList().push_back(body);
			context->root->builder.SetInsertPoint(body);

			context->PushLoop(end, start);
			this->block->Compile(context);
			context->PopLoop();

			context->root->builder.CreateBr(start);

			context->f->getBasicBlockList().push_back(end);
			context->root->builder.SetInsertPoint(end);

			return CValue();
		}

		void CompileDeclarations(CompilerContext* context) {};

		void Print(std::string& output, Source* source)
		{
			//add tokens for the ( )
			token.Print(output, source);
			output += "(";
			this->condition->Print(output, source);
			output += ")";

			this->block->Print(output, source);
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);
			condition->Visit(visitor);
			block->Visit(visitor);
		}
	};

	class ForExpression : public Expression
	{
		Expression* condition, *initial, *incr;
		ScopeExpression* block;
		Token token;
	public:
		ForExpression(Token token, Expression* init, Expression* cond, Expression* incr, ScopeExpression* block)
		{
			this->condition = cond;
			this->block = block;
			this->incr = incr;
			this->initial = init;
			this->token = token;
		}

		~ForExpression()
		{
			delete this->condition;
			delete this->block;
			delete this->incr;
			delete this->initial;
		}

		void SetParent(Expression* parent)
		{
			this->Parent = parent;
			block->SetParent(this);
			incr->SetParent(block);//block);
			condition->SetParent(this);//block);
			initial->SetParent(block);
		}

		CValue Compile(CompilerContext* context)
		{
			context->CurrentToken(&token);

			this->initial->Compile(context);

			llvm::BasicBlock *start = llvm::BasicBlock::Create(llvm::getGlobalContext(), "forstart");
			llvm::BasicBlock *body = llvm::BasicBlock::Create(llvm::getGlobalContext(), "forbody");
			llvm::BasicBlock *end = llvm::BasicBlock::Create(llvm::getGlobalContext(), "forend");
			llvm::BasicBlock *cont = llvm::BasicBlock::Create(llvm::getGlobalContext(), "forcontinue");

			//insert stupid branch
			context->root->builder.CreateBr(start);
			context->f->getBasicBlockList().push_back(start);
			context->root->builder.SetInsertPoint(start);

			auto cond = this->condition->Compile(context);
			cond = context->DoCast(context->root->BoolType, cond);

			context->root->builder.CreateCondBr(cond.val, body, end);

			context->f->getBasicBlockList().push_back(body);
			context->root->builder.SetInsertPoint(body);

			context->PushLoop(end, cont);
			this->block->Compile(context);
			context->PopLoop();

			context->root->builder.CreateBr(cont);

			//insert continue branch here
			context->f->getBasicBlockList().push_back(cont);
			context->root->builder.SetInsertPoint(cont);

			this->incr->Compile(context);

			context->root->builder.CreateBr(start);

			context->f->getBasicBlockList().push_back(end);
			context->root->builder.SetInsertPoint(end);

			return CValue();
		}

		void CompileDeclarations(CompilerContext* context) {};

		void Print(std::string& output, Source* source)
		{
			//add tokens for the ( )
			token.Print(output, source);
			output += "(";
			this->initial->Print(output, source);
			output += ";";
			this->condition->Print(output, source);
			output += ";";
			this->incr->Print(output, source);
			output += ")";

			this->block->Print(output, source);
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);
			initial->Visit(visitor);
			condition->Visit(visitor);
			incr->Visit(visitor);
			block->Visit(visitor);
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

	class CaseExpression : public Expression
	{

		Token token;
	public:
		int value;
		CaseExpression(Token token, int value)
		{
			this->value = value;
			this->token = token;
		}

		~CaseExpression()
		{
		}

		virtual void SetParent(Expression* parent)
		{
			this->Parent = parent;
		}

		CValue Compile(CompilerContext* context);

		void CompileDeclarations(CompilerContext* context) {};

		void Print(std::string& output, Source* source)
		{
			//add tokens for the ( )
			token.Print(output, source);

			throw 7; //todo
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);
		}
	};

	class DefaultExpression : public Expression
	{
		Token token;
	public:
		DefaultExpression(Token token)
		{
			this->token = token;
		}

		~DefaultExpression()
		{
		}

		virtual void SetParent(Expression* parent)
		{
			this->Parent = parent;
		}

		CValue Compile(CompilerContext* context);

		void CompileDeclarations(CompilerContext* context) {};

		void Print(std::string& output, Source* source)
		{
			//add tokens for the ( )
			token.Print(output, source);

			throw 7; //todo
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);
		}
	};

	class SwitchExpression : public Expression
	{
		Expression* var;
		BlockExpression* block;
		Token token;
		llvm::SwitchInst* sw;
	public:

		llvm::BasicBlock* def;
		llvm::BasicBlock* switch_end;

		bool first_case;
		SwitchExpression(Token token, Expression* var, BlockExpression* block)
		{
			this->first_case = true;
			this->var = var;
			this->block = block;
			this->token = token;
			this->def = 0;
		}

		~SwitchExpression()
		{
			delete block;
		}

		bool AddCase(llvm::ConstantInt* value, llvm::BasicBlock* dest)
		{
			bool tmp = this->first_case;
			this->first_case = false;
			this->sw->addCase(value, dest);
			return tmp;
		}

		bool AddDefault(llvm::BasicBlock* dest)
		{
			bool tmp = this->first_case;
			this->first_case = false;
			this->def = dest;
			return tmp;
		}

		virtual void SetParent(Expression* parent)
		{
			this->Parent = parent;
			this->var->SetParent(this);
			this->block->SetParent(this);
		}

		CValue Compile(CompilerContext* context);

		void CompileDeclarations(CompilerContext* context) {};

		void Print(std::string& output, Source* source)
		{
			//add tokens for the ( )
			token.Print(output, source);
			output += "(";
			var->Print(output, source);
			output += ")";

			this->block->Print(output, source);
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);
			var->Visit(visitor);
			block->Visit(visitor);
		}
	};



	struct Branch
	{
		Token token;
		BlockExpression* block;
		Expression* condition;

		Branch(Token token, BlockExpression* block, Expression* condition)
		{
			this->token = token;
			this->block = block;
			this->condition = condition;
		}

		Branch(Branch&& other)
		{
			this->block = other.block;
			this->condition = other.condition;
			other.block = 0;
			other.condition = 0;
		}
		~Branch()
		{
			delete condition;
			delete block;
		}
	};
	class IfExpression : public Expression
	{
		std::vector<Branch*> branches;
		Branch* Else;
		Token token;
	public:
		IfExpression(Token token, std::vector<Branch*>&& branches, Branch* elseBranch)
		{
			this->branches = branches;
			this->Else = elseBranch;
			this->token = token;
		}

		~IfExpression()
		{
			delete Else;
			for (auto ii : this->branches)
				delete ii;
		}

		virtual void SetParent(Expression* parent)
		{
			this->Parent = parent;
			if (this->Else)
				this->Else->block->SetParent(this);
			for (auto& ii : branches)
			{
				ii->block->SetParent(this);
				ii->condition->SetParent(this);
			}
		}

		CValue Compile(CompilerContext* context)
		{
			context->CurrentToken(&token);

			int pos = 0;
			bool hasElse = this->Else ? this->Else->block->statements.size() > 0 : false;
			llvm::BasicBlock *EndBB = llvm::BasicBlock::Create(llvm::getGlobalContext(), "endif");
			llvm::BasicBlock *ElseBB = 0;
			if (hasElse)
				ElseBB = llvm::BasicBlock::Create(llvm::getGlobalContext(), "else");

			llvm::BasicBlock *NextBB = 0;
			for (auto& ii : this->branches)
			{
				if (NextBB)
					context->root->builder.SetInsertPoint(NextBB);

				auto cond = ii->condition->Compile(context);
				cond = context->DoCast(context->root->BoolType, cond);//try and cast to bool

				llvm::BasicBlock *ThenBB = llvm::BasicBlock::Create(llvm::getGlobalContext(), "then", context->f);
				NextBB = pos == (branches.size() - 1) ? (hasElse ? ElseBB : EndBB) : llvm::BasicBlock::Create(llvm::getGlobalContext(), "elseif", context->f);

				context->root->builder.CreateCondBr(cond.val, ThenBB, NextBB);

				//statement body
				context->root->builder.SetInsertPoint(ThenBB);
				ii->block->Compile(context);
				if (context->root->builder.GetInsertBlock()->getTerminator() == 0)
					context->root->builder.CreateBr(EndBB);//branch to end

				pos++;
			}

			if (hasElse)
			{
				context->f->getBasicBlockList().push_back(ElseBB);
				context->root->builder.SetInsertPoint(ElseBB);

				this->Else->block->Compile(context);
				context->root->builder.CreateBr(EndBB);
			}
			context->f->getBasicBlockList().push_back(EndBB);
			context->root->builder.SetInsertPoint(EndBB);

			return CValue();
		}

		void CompileDeclarations(CompilerContext* context) {};

		void Print(std::string& output, Source* source)
		{
			//this->token.Print(output, source);

			for (auto ii : this->branches)
			{
				ii->token.Print(output, source);
				output += " (";
				ii->condition->Print(output, source);
				output += ")";
				ii->block->Print(output, source);
			}
			if (this->Else)
			{
				this->Else->token.Print(output, source);

				//this->Else->condition->Print(output, source);
				this->Else->block->Print(output, source);
			}
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);

			for (auto ii : this->branches)
			{
				ii->condition->Visit(visitor);
				ii->block->Visit(visitor);
			}
			if (this->Else)
			{
				this->Else->block->Visit(visitor);
			}
		}
	};

	class SizeofExpression : public Expression//, public IStorableExpression
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
			this->Parent = parent;
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
	};

	class NamespaceExpression : public Expression//, public IStorableExpression
	{
		Token begin, end;
		Token name;
		Token token;
		BlockExpression* block;
	public:
		NamespaceExpression(Token token, Token name, Token start, BlockExpression* block, Token end)
		{
			this->name = name;
			this->begin = start;
			this->token = token;
			this->end = end;
			this->block = block;
		}

		void SetParent(Expression* parent)
		{
			this->Parent = parent;
			this->block->SetParent(this);
		}

		CValue Compile(CompilerContext* context)
		{
			context->SetNamespace(this->name.text);

			this->block->Compile(context);

			context->PopNamespace();

			return CValue();
		}

		void CompileDeclarations(CompilerContext* context)
		{
			context->SetNamespace(this->name.text);

			this->block->CompileDeclarations(context);

			context->PopNamespace();
		}

		void Print(std::string& output, Source* source)
		{
			token.Print(output, source);
			name.Print(output, source);
			begin.Print(output, source);
			block->Print(output, source);
			end.Print(output, source);
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);
		}
	};

	class CallExpression : public Expression
	{
		Token token;


	public:
		Expression* left;
		std::vector<Expression*>* args;
		friend class FunctionParselet;
		CallExpression(Token token, Expression* left, std::vector<Expression*>* args)
		{
			this->token = token;
			this->left = left;
			this->args = args;
		}

		~CallExpression()
		{
			delete this->left;
			if (args)
			{
				for (auto ii : *args)
					delete ii;

				delete args;
			}
		}

		void SetParent(Expression* parent)
		{
			this->Parent = parent;
			left->SetParent(this);
			for (auto ii : *args)
				ii->SetParent(this);
		}

		CValue Compile(CompilerContext* context);

		Type* GetReturnType()
		{
			return 0;
		}

		void CompileDeclarations(CompilerContext* context) {};

		void Print(std::string& output, Source* source)
		{
			//add tokens for the ( )
			this->left->Print(output, source);

			token.Print(output, source);
			int i = 0;
			for (auto ii : *this->args)
			{
				ii->Print(output, source);

				if (i != args->size() - 1)
					output += ",";
				i++;
			}
			output += ")";
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);
			left->Visit(visitor);
			if (this->args)
			{
				for (auto ii : *this->args)
					ii->Visit(visitor);
			}
		}
	};

	class FunctionExpression : public Expression
	{
		friend class Namespace;
		friend class Compiler;
		friend class CompilerContext;
		friend class Type;
		friend class Function;
		friend struct Struct;
		Token name;
		std::vector<std::pair<std::string, std::string>>* args;
		std::vector<Token>* captures;
		ScopeExpression* block;
		Token token;

		Token Struct;

		Token ret_type;
		NameExpression* varargs;

		std::vector<std::pair<Token, Token>>* templates;
	public:

		ScopeExpression* GetBlock()
		{
			return block;
		}

		FunctionExpression(Token token, Token name, Token ret_type, std::vector<std::pair<std::string, std::string>>* args, ScopeExpression* block, /*NameExpression* varargs = 0,*/ Token Struct, std::vector<std::pair<Token, Token>>* templates, std::vector<Token>* captures = 0)
		{
			this->ret_type = ret_type;
			this->args = args;
			this->block = block;
			this->name = name;
			this->token = token;
			this->varargs = 0;// varargs;
			this->Struct = Struct;
			this->templates = templates;
			this->captures = captures;
		}

		~FunctionExpression()
		{
			delete block;
			delete varargs;
			delete args;
			delete captures;
		}

		std::string GetRealName();

		std::string GetName()
		{
			return this->name.text;
		}

		void SetParent(Expression* parent)
		{
			this->Parent = parent;
			block->SetParent(this);
		}

		CValue Compile(CompilerContext* context);
		void CompileDeclarations(CompilerContext* context);

		void Print(std::string& output, Source* source)
		{
			//add tokens for the ( )
			token.Print(output, source);

			ret_type.Print(output, source);

			name.Print(output, source);

			output += "(";
			int i = 0;
			for (auto ii : *this->args)
			{
				output += ii.first + " " + ii.second;
				if (i != args->size() - 1)
					output += ", ";
				//ii->Print(output, source);
				i++;
			}
			//this->condition->Print(output, source);
			output += ")";

			this->block->Print(output, source);
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);
			block->Visit(visitor);
		}

		CValue FunctionExpression::DoCompile(CompilerContext* context);//call this to do real compilation
	};

	class ExternExpression : public Expression
	{
		Token name;
		std::string Struct;
		std::vector<std::pair<std::string, std::string>>* args;
		Token token;
		Token ret_type;
	public:

		ExternExpression(Token token, Token name, Token ret_type, std::vector<std::pair<std::string, std::string>>* args, std::string str = "")
		{
			this->args = args;
			this->name = name;
			this->token = token;
			this->ret_type = ret_type;
			this->Struct = str;
		}

		~ExternExpression()
		{
			delete args;
		}

		void SetParent(Expression* parent)
		{
			this->Parent = parent;
		}

		CValue Compile(CompilerContext* context);

		void CompileDeclarations(CompilerContext* context);

		void Print(std::string& output, Source* source)
		{
			//add tokens for the ( )
			token.Print(output, source);

			output += " fun";
			ret_type.Print(output, source);

			name.Print(output, source);

			output += "(";
			int i = 0;
			for (auto ii : *this->args)
			{
				output += ii.first + " " + ii.second;
				if (i != args->size() - 1)
					output += ", ";
				//ii->Print(output, source);
				i++;
			}
			output += ")";
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);
		}
	};

	struct TraitFunction
	{
		Token name;
		Token ret_type;

		std::vector<Token> args;
	};

	class TraitExpression : public Expression
	{
		Token token;
		Token name;
		std::vector<TraitFunction> funcs;
		std::vector<std::pair<Token, Token>>* templates;
	public:
		TraitExpression(Token token, Token name, std::vector<TraitFunction>&& funcs, std::vector<std::pair<Token, Token>>* templates)
		{
			this->token = token;
			this->name = name;
			this->funcs = funcs;
			this->templates = templates;
		}

		CValue Compile(CompilerContext* context)
		{
			return CValue();
		}

		void CompileDeclarations(CompilerContext* context);

		void Print(std::string& output, Source* source)
		{
			token.Print(output, source);
			name.Print(output, source);

			output += "{";

			for (auto fun : this->funcs)
			{
				output += " fun ";
				fun.ret_type.Print(output, source);
				output += " ";
				fun.name.Print(output, source);
				output += "(";
				bool first = false;
				for (auto arg : fun.args)
				{
					if (first)
						output += ", ";
					else
						first = true;

					arg.Print(output, source);
					//output += arg.first->ToString() + " " + arg.second;
				}
				output += ");";
			}
			output += "}";
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);
		}
	};

	struct StructMember
	{
		enum MemberType
		{
			FunctionMember,
			VariableMember,
		};
		MemberType type;

		FunctionExpression* function;
		std::pair<Token, Token> variable;
	};
	class StructExpression : public Expression
	{
		friend class Compiler;
		friend class Type;

		Token token;
		Token name;

		std::vector<std::pair<Token, Token>>* templates;

		Token start;
		Token end;

		Token base_type;

		void AddConstructorDeclarations(Type* str, CompilerContext* context);
		void AddConstructors(CompilerContext* context);
	public:

		std::vector<StructMember> members;

		StructExpression(Token token, Token name, Token start, Token end, std::vector<StructMember>&& members, /*std::vector<std::pair<std::string, std::string>>* elements, std::vector<FunctionExpression*>* functions,*/ std::vector<std::pair<Token, Token>>* templates, Token base_type)
		{
			this->templates = templates;
			this->members = members;
			this->base_type = base_type;
			this->name = name;
			this->token = token;
			this->start = start;
			this->end = end;
		}

		~StructExpression()
		{
			for (auto ii : members)
				if (ii.type == StructMember::FunctionMember)
					delete ii.function;
		}

		std::string GetName()
		{
			return this->name.text;
		}

		void SetParent(Expression* parent)
		{
			this->Parent = parent;
			for (auto ii : this->members)
			{
				if (ii.type == StructMember::FunctionMember)
					ii.function->SetParent(this);
			}
		}

		CValue Compile(CompilerContext* context);

		void CompileDeclarations(CompilerContext* context);

		void Print(std::string& output, Source* source)
		{
			//add tokens for the ( )
			token.Print(output, source);

			name.Print(output, source);
			//output templates
			if (this->templates && this->templates->size() > 0)
			{
				output += "<";

				int i = 0;
				for (auto ii : *this->templates)
				{
					ii.first.Print(output, source);
					ii.second.Print(output, source);
					if (i != this->templates->size() - 1)
						output += ",";
					i++;
				}
				output += ">";
			}
			this->start.Print(output, source);

			for (auto ii : this->members)
			{
				if (ii.type == StructMember::FunctionMember)
					ii.function->Print(output, source);
				else
				{
					ii.variable.first.Print(output, source);
					ii.variable.second.Print(output, source);
					output += ";";
				}
			}

			this->end.Print(output, source);
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);

			for (auto ii : members)
			{
				if (ii.type == StructMember::FunctionMember)
					ii.function->Visit(visitor);
			}
		}
	};

	class ReturnExpression : public Expression
	{
		Token token;
		Expression* right;
	public:

		ReturnExpression(Token token, Expression* right)
		{
			this->token = token;
			this->right = right;
		}

		~ReturnExpression()
		{
			delete this->right;
		}

		void SetParent(Expression* parent)
		{
			this->Parent = parent;
			if (right)
				this->right->SetParent(this);
		}

		CValue Compile(CompilerContext* context)
		{
			context->CurrentToken(&token);
			context->SetDebugLocation(token);

			if (right)
				context->Return(right->Compile(context));
			else
				context->root->builder.CreateRetVoid();

			return CValue();
		}

		void CompileDeclarations(CompilerContext* context) {};

		void Print(std::string& output, Source* source)
		{
			token.Print(output, source);
			if (right)
				right->Print(output, source);
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);
			if (right)
				right->Visit(visitor);
		}
	};

	class BreakExpression : public Expression
	{
		Token token;
	public:
		BreakExpression(Token token) : token(token) {}

		void SetParent(Expression* parent)
		{
			this->Parent = parent;
		}

		CValue Compile(CompilerContext* context)
		{
			context->CurrentToken(&token);
			context->SetDebugLocation(token);
			context->Break();

			return CValue();
		}

		void CompileDeclarations(CompilerContext* context) {};

		void Print(std::string& output, Source* source)
		{
			token.Print(output, source);
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);
		}
	};

	class ContinueExpression : public Expression
	{
		Token token;
	public:
		ContinueExpression(Token token) : token(token) {}

		void SetParent(Expression* parent)
		{
			this->Parent = parent;
		}

		CValue Compile(CompilerContext* context)
		{
			context->CurrentToken(&token);
			context->SetDebugLocation(token);
			context->Continue();

			return CValue();
		}

		void CompileDeclarations(CompilerContext* context) {};

		void Print(std::string& output, Source* source)
		{
			token.Print(output, source);
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);
		}
	};

	class TypedefExpression : public Expression
	{
		Token token;
		Token new_type;
		Token equals;
		Token other_type;
	public:
		TypedefExpression(Token token, Token new_type, Token equals, Token other_type) : token(token), new_type(new_type),
			equals(equals), other_type(other_type) {}

		void SetParent(Expression* parent)
		{
			this->Parent = parent;
		}

		CValue Compile(CompilerContext* context)
		{
			if (this->Parent->Parent != 0)
				context->root->Error("Cannot use typedef outside of global scope", token);

			return CValue();
		}

		void CompileDeclarations(CompilerContext* context)
		{
			if (this->Parent->Parent != 0)
				context->root->Error("Cannot use typedef outside of global scope", token);

			context->root->ns->members.insert({this->new_type.text, context->root->LookupType(this->other_type.text)});
			//context->root->Error("Typedef not implemented atm", token);

			/*context->CurrentToken(&other_type);
			auto type = context->root->AdvanceTypeLookup(other_type.text);

			//this can be fixed by adding a third pass over the typedefs
			auto iter = context->root->types.find(new_type.text);
			if (iter != context->root->types.end())
			{
			if (iter->second->type == Types::Invalid)
			context->root->Error("Please place the typedef before it is used, after is not yet handled properly.", new_type);
			else
			context->root->Error("Type '" + new_type.text + "' already defined!", new_type);
			}
			context->root->types[new_type.text] = type;*/
		};

		void Print(std::string& output, Source* source)
		{
			token.Print(output, source);
			new_type.Print(output, source);
			equals.Print(output, source);
			other_type.Print(output, source);
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);
		}
	};

	/*class YieldExpression: public Expression
	{
	Token token;
	Expression* right;
	public:
	YieldExpression(Token t, Expression* right)
	{
	this->token = t;
	this->right = right;
	}

	void SetParent(Expression* parent)
	{
	this->Parent = parent;
	if (right)
	right->SetParent(this);
	}

	void Compile(CompilerContext* context)
	{
	if (right)
	right->Compile(context);
	else
	context->Null();

	context->Yield();

	if (dynamic_cast<BlockExpression*>(this->Parent) !=0)
	context->Pop();
	}
	};

	class ResumeExpression: public Expression
	{
	Token token;
	Expression* right;
	public:
	ResumeExpression(Token t, Expression* right)
	{
	this->token = t;
	this->right = right;
	}

	void SetParent(Expression* parent)
	{
	this->Parent = parent;
	if (right)
	right->SetParent(this);
	}

	void Compile(CompilerContext* context)
	{
	this->right->Compile(context);

	context->Resume();

	if (dynamic_cast<BlockExpression*>(this->Parent))
	context->Pop();
	}
	};*/
}
#endif