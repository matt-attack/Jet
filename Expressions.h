#ifndef _EXPRESSIONS_HEADER
#define _EXPRESSIONS_HEADER

#include <string>
#include <stdio.h>
#include <vector>

#include "Compiler.h"

namespace Jet
{
	class Compiler;

	class Expression
	{
	public:

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
	};

	class IStorableExpression
	{
	public:
		virtual void CompileStore(CompilerContext* context, CValue right) = 0;
	};

	class NameExpression : public Expression, public IStorableExpression
	{
		Token token;
	public:
		NameExpression(Token name)
		{
			this->token = name;
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
		std::vector<std::pair<std::string, Token>>* _names;
		std::vector<Expression*>* _right;
	public:
		LocalExpression(std::vector<std::pair<std::string, Token>>* names, std::vector<Expression*>* right)
		{
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
	};

	class NumberExpression : public Expression
	{
		double value;
	public:
		NumberExpression(double value)
		{
			this->value = value;
		}

		double GetValue()
		{
			return this->value;
		}

		CValue Compile(CompilerContext* context);

		void CompileDeclarations(CompilerContext* context) {};
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
		std::string value;
	public:
		StringExpression(std::string value)
		{
			this->value = value;
		}

		std::string GetValue()
		{
			return this->value;
		}

		CValue Compile(CompilerContext* context);

		void CompileDeclarations(CompilerContext* context) {};
	};

	class IndexExpression : public Expression, public IStorableExpression
	{
		
	public:
		Expression* index;
		Expression* left;
		Token token;
		IndexExpression(Expression* left, Expression* index, Token t)
		{
			this->token = t;
			this->left = left;
			this->index = index;
		}

		~IndexExpression()
		{
			delete left;
			delete index;
		}

		CValue Compile(CompilerContext* context);

		CValue GetGEP(CompilerContext* context);
		CValue GetBaseGEP(CompilerContext* context);

		Type* GetType(CompilerContext* context);
		Type* GetBaseType(CompilerContext* context);

		void CompileStore(CompilerContext* context, CValue right);

		void CompileDeclarations(CompilerContext* context) {};
	};

	class AssignExpression : public Expression
	{
		Expression* left;
		Expression* right;
	public:
		AssignExpression(Expression* l, Expression* r)
		{
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
				throw 7;//invalid atm

			if (this->_operator.type == TokenType::Asterisk)
			{
				auto i = dynamic_cast<NameExpression*>(this->right);
				auto p = dynamic_cast<IndexExpression*>(this->right);
				if (i)
				{
					//auto var = context->named_values[i->GetName()];
					auto var = context->Load(i->GetName());
					auto val = context->parent->builder.CreateLoad(var.val);
					
					//context->Store(i->GetName(), val);
					right = context->DoCast(var.type->base, right);

					context->parent->builder.CreateStore(right.val, val);

					return;
				}
				else if (p)
				{
					auto var = p->GetGEP(context);
					auto val = context->parent->builder.CreateLoad(var.val);

					right = context->DoCast(var.type->base, right);

					context->parent->builder.CreateStore(right.val, val);
					return;
				}
				throw 7;
			}
			throw 7;//implement me
		}

		CValue Compile(CompilerContext* context);

		void CompileDeclarations(CompilerContext* context) {};
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
	};

	class StatementExpression : public Expression
	{
	public:
	};

	class BlockExpression : public Expression
	{

	public:
		std::vector<Expression*> statements;
		BlockExpression(Token token, std::vector<Expression*>&& statements) : statements(statements)
		{

		}

		BlockExpression(std::vector<Expression*>&& statements) : statements(statements)
		{

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
				ii->Compile(context);

			return CValue();
		}

		void CompileDeclarations(CompilerContext* context)
		{
			for (auto ii : statements)
				ii->CompileDeclarations(context);
		};
	};

	class ScopeExpression : public BlockExpression
	{

	public:
		//add a list of local variables here mayhaps?

		ScopeExpression(BlockExpression* r)
		{
			this->statements = r->statements;
			r->statements.clear();
			delete r;
		}

		CValue Compile(CompilerContext* context)
		{
			//push scope
			context->PushScope();

			CValue tmp = BlockExpression::Compile(context);

			//pop scope
			context->PopScope();

			return tmp;
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


			context->parent->builder.CreateBr(start);
			context->f->getBasicBlockList().push_back(start);
			context->parent->builder.SetInsertPoint(start);

			auto cond = this->condition->Compile(context);
			cond = context->DoCast(&BoolType, cond);
			context->parent->builder.CreateCondBr(cond.val, body, end);


			context->f->getBasicBlockList().push_back(body);
			context->parent->builder.SetInsertPoint(body);

			context->PushLoop(end, start);
			this->block->Compile(context);
			context->PopLoop();

			context->parent->builder.CreateBr(start);

			context->f->getBasicBlockList().push_back(end);
			context->parent->builder.SetInsertPoint(end);

			return CValue();
		}

		void CompileDeclarations(CompilerContext* context) {};
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
			context->parent->builder.CreateBr(start);
			context->f->getBasicBlockList().push_back(start);
			context->parent->builder.SetInsertPoint(start);

			auto cond = this->condition->Compile(context);
			cond = context->DoCast(&BoolType, cond);

			context->parent->builder.CreateCondBr(cond.val, body, end);

			context->f->getBasicBlockList().push_back(body);
			context->parent->builder.SetInsertPoint(body);

			context->PushLoop(end, cont);
			this->block->Compile(context);
			context->PopLoop();

			context->parent->builder.CreateBr(cont);

			//insert continue branch here
			context->f->getBasicBlockList().push_back(cont);
			context->parent->builder.SetInsertPoint(cont);

			this->incr->Compile(context);

			context->parent->builder.CreateBr(start);

			context->f->getBasicBlockList().push_back(end);
			context->parent->builder.SetInsertPoint(end);

			return CValue();
		}

		void CompileDeclarations(CompilerContext* context) {};
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
	};

	struct Branch
	{
		BlockExpression* block;
		Expression* condition;

		Branch(BlockExpression* block, Expression* condition)
		{
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
					context->parent->builder.SetInsertPoint(NextBB);

				auto cond = ii->condition->Compile(context);
				cond = context->DoCast(&BoolType, cond);//try and cast to bool

				llvm::BasicBlock *ThenBB = llvm::BasicBlock::Create(llvm::getGlobalContext(), "then", context->f);
				NextBB = pos == (branches.size() - 1) ? (hasElse ? ElseBB : EndBB) : llvm::BasicBlock::Create(llvm::getGlobalContext(), "elseif", context->f);

				context->parent->builder.CreateCondBr(cond.val, ThenBB, NextBB);

				//statement body
				context->parent->builder.SetInsertPoint(ThenBB);
				ii->block->Compile(context);
				context->parent->builder.CreateBr(EndBB);//branch to end

				pos++;
			}

			if (hasElse)
			{
				context->f->getBasicBlockList().push_back(ElseBB);
				context->parent->builder.SetInsertPoint(ElseBB);

				this->Else->block->Compile(context);
				context->parent->builder.CreateBr(EndBB);
			}
			context->f->getBasicBlockList().push_back(EndBB);
			context->parent->builder.SetInsertPoint(EndBB);

			return CValue();
		}

		void CompileDeclarations(CompilerContext* context) {};
	};

	class CallExpression : public Expression
	{
		Token token;
		Expression* left;
		std::vector<Expression*>* args;
	public:
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

		void CompileDeclarations(CompilerContext* context) {};
	};

	class FunctionExpression : public Expression
	{
		Token name;
		std::vector<std::pair<std::string, std::string>>* args;
		ScopeExpression* block;
		Token token;

		Token Struct;

		std::string ret_type;
		NameExpression* varargs;
	public:

		FunctionExpression(Token token, Token name, std::string& ret_type, std::vector<std::pair<std::string, std::string>>* args, ScopeExpression* block, /*NameExpression* varargs = 0,*/ Token Struct)
		{
			this->ret_type = ret_type;
			this->args = args;
			this->block = block;
			this->name = name;
			this->token = token;
			this->varargs = 0;// varargs;
			this->Struct = Struct;
		}

		~FunctionExpression()
		{
			delete block;
			delete varargs;
			delete args;
		}

		std::string GetRealName();

		void MakeMemberFunction()
		{

		}

		void SetParent(Expression* parent)
		{
			this->Parent = parent;
			block->SetParent(this);
		}

		CValue Compile(CompilerContext* context);

		void CompileDeclarations(CompilerContext* context);
	};

	class ExternExpression : public Expression
	{
		Token name;
		std::string Struct;
		std::vector<std::pair<std::string, std::string>>* args;
		Token token;
		std::string ret_type;
	public:

		ExternExpression(Token token, Token name, std::string& ret_type, std::vector<std::pair<std::string, std::string>>* args, std::string str = "")
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
	};

	class StructExpression : public Expression
	{
		std::string name;
		std::vector<std::pair<std::string, std::string>>* elements;
		std::vector<FunctionExpression*>* functions;
		Token token;
		std::string ret_type;
	public:

		StructExpression(Token token, std::string& name, std::vector<std::pair<std::string, std::string>>* elements, std::vector<FunctionExpression*>* functions)
		{
			this->elements = elements;
			this->name = name;
			this->token = token;
			this->ret_type = ret_type;
			this->functions = functions;
		}

		~StructExpression()
		{
			if (functions)
				for (auto fun : *functions)
					delete fun;
			delete functions;
			delete elements;
		}

		std::string GetName()
		{
			return this->name;
		}

		void SetParent(Expression* parent)
		{
			this->Parent = parent;
			for (auto ii : *this->functions)
			{
				ii->SetParent(this);
			}
		}

		CValue Compile(CompilerContext* context);

		void CompileDeclarations(CompilerContext* context);
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

			if (right)
				context->Return(right->Compile(context));
			else
				context->parent->builder.CreateRetVoid();

			return CValue();
		}

		void CompileDeclarations(CompilerContext* context) {};
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
			context->Break();

			return CValue();
		}

		void CompileDeclarations(CompilerContext* context) {};
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
			context->Continue();

			return CValue();
		}

		void CompileDeclarations(CompilerContext* context) {};
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