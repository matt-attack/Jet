#ifndef _EXPRESSIONS_HEADER
#define _EXPRESSIONS_HEADER

#include <string>
#include <stdio.h>
#include <vector>

#include "Compiler.h"
//#include "Value.h"

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
		std::string name;
	public:
		NameExpression(std::string name)
		{
			this->name = name;
		}

		std::string GetName()
		{
			return this->name;
		}

		CValue Compile(CompilerContext* context);

		void CompileStore(CompilerContext* context, CValue right)
		{
			//need to do cast if necessary
			right = context->DoCast(context->named_values[name].type, right);
			context->Store(name, right);
			//context->Store(name);
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
		Expression*index;
	public:
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

	/*class PrefixExpression: public Expression
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

	llvm::Value* Compile(CompilerContext* context);
	};

	class PostfixExpression: public Expression
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

	llvm::Value* Compile(CompilerContext* context);
	};*/

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
			//context->PushScope();

			return BlockExpression::Compile(context);

			//pop scope
			//context->PopScope();
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
			/*context->Line(token.line);

			std::string uuid = context->GetUUID();
			context->Label("loopstart_"+uuid);
			this->condition->Compile(context);
			context->JumpFalse(("loopend_"+uuid).c_str());

			context->PushLoop("loopend_"+uuid, "loopstart_"+uuid);
			this->block->Compile(context);
			context->PopLoop();

			context->Jump(("loopstart_"+uuid).c_str());
			context->Label("loopend_"+uuid);*/
			throw 7;
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

		/*	int pos = 0;
			bool hasElse = this->Else ? this->Else->block->statements.size() > 0 : false;
			llvm::BasicBlock *EndBB = llvm::BasicBlock::Create(llvm::getGlobalContext(), "endif");
			llvm::BasicBlock *ElseBB = 0;
			if (hasElse)
			ElseBB = llvm::BasicBlock::Create(llvm::getGlobalContext(), "else");

			llvm::BasicBlock *NextBB = 0;
			for (auto& ii : this->branches)
			{
			auto cond = ii->condition->Compile(context);

			cond = context->DoCast(Type(Types::Bool), cond);//try and cast to bool
			//	*/
		CValue Compile(CompilerContext* context)
		{
			context->Line(token.line);


			//std::string uuid = context->GetUUID();
			this->initial->Compile(context);
			//context->Label("forloopstart_"+uuid);

			llvm::BasicBlock *start = llvm::BasicBlock::Create(llvm::getGlobalContext(), "forstart");
			llvm::BasicBlock *body = llvm::BasicBlock::Create(llvm::getGlobalContext(), "forbody");
			llvm::BasicBlock *end = llvm::BasicBlock::Create(llvm::getGlobalContext(), "forend");

			//insert stupid branch
			context->parent->builder.CreateBr(start);
			context->f->getBasicBlockList().push_back(start);
			context->parent->builder.SetInsertPoint(start);

			auto cond = this->condition->Compile(context);
			cond = context->DoCast(&BoolType, cond);

			context->parent->builder.CreateCondBr(cond.val, body, end);

			//context->JumpFalse(("forloopend_"+uuid).c_str());

			context->f->getBasicBlockList().push_back(body);
			context->parent->builder.SetInsertPoint(body);

			//context->PushLoop("forloopend_"+uuid, "forloopcontinue_"+uuid);
			this->block->Compile(context);
			//context->PopLoop();

			//this wont work if we do some kind of continue keyword unless it jumps to here
			//context->Label("forloopcontinue_"+uuid);
			this->incr->Compile(context);

			context->parent->builder.CreateBr(start);

			//context->Jump(("forloopstart_"+uuid).c_str());
			//context->Label("forloopend_"+uuid);
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
			//delete this->branches;
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
			context->Line(token.line);

			//std::string uuid = context->GetUUID();
			//std::string bname = "ifstatement_" + uuid + "_I";
			int pos = 0;
			bool hasElse = this->Else ? this->Else->block->statements.size() > 0 : false;
			llvm::BasicBlock *EndBB = llvm::BasicBlock::Create(llvm::getGlobalContext(), "endif");
			llvm::BasicBlock *ElseBB = 0;
			if (hasElse)
				ElseBB = llvm::BasicBlock::Create(llvm::getGlobalContext(), "else");

			llvm::BasicBlock *NextBB = 0;
			for (auto& ii : this->branches)
			{
				auto cond = ii->condition->Compile(context);

				cond = context->DoCast(&BoolType, cond);//try and cast to bool
				//	need to convert to a type usable for this
				//cond = CValue(Types::Bool, context->parent->builder.CreateIsNotNull(cond.val));//fixme later,

				llvm::BasicBlock *ThenBB = llvm::BasicBlock::Create(llvm::getGlobalContext(), "then", context->f);
				NextBB = hasElse ? ElseBB : EndBB;
				//if (hasElse)
				//NextBB = ElseBB;// llvm::BasicBlock::Create(llvm::getGlobalContext(), "else");

				context->parent->builder.CreateCondBr(cond.val, ThenBB, NextBB);

				context->parent->builder.SetInsertPoint(ThenBB);

				ii->block->Compile(context);

				context->parent->builder.CreateBr(EndBB);//branch to end

				pos++;
				break;
			}

			if (hasElse)//this->Else && this->Else->block->statements->size() > 0)
			{
				context->f->getBasicBlockList().push_back(ElseBB);
				context->parent->builder.SetInsertPoint(ElseBB);
				//context->Label(bname);
				this->Else->block->Compile(context);
				context->parent->builder.CreateBr(EndBB);
			}
			context->f->getBasicBlockList().push_back(EndBB);
			context->parent->builder.SetInsertPoint(EndBB);
			//context->Label("ifstatementend_"+uuid);
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
		Expression* name;
		std::vector<std::pair<std::string, std::string>>* args;
		ScopeExpression* block;
		Token token;

		std::string ret_type;
		NameExpression* varargs;
	public:

		FunctionExpression(Token token, Expression* name, std::string& ret_type, std::vector<std::pair<std::string, std::string>>* args, ScopeExpression* block, NameExpression* varargs = 0)
		{
			this->ret_type = ret_type;
			this->args = args;
			this->block = block;
			this->name = name;
			this->token = token;
			this->varargs = varargs;
		}

		~FunctionExpression()
		{
			delete block;
			delete name;
			delete varargs;

			if (args)
			{
				//for (auto ii: *args)
				//delete ii;
				delete args;
			}
		}

		void SetParent(Expression* parent)
		{
			this->Parent = parent;
			block->SetParent(this);
			if (name)
				name->SetParent(this);
			//for (auto ii: *args)
			//ii->SetParent(this);
		}

		CValue Compile(CompilerContext* context);

		void CompileDeclarations(CompilerContext* context);
	};

	class ExternExpression : public Expression
	{
		Expression* name;
		std::vector<std::pair<std::string, std::string>>* args;
		Token token;
		std::string ret_type;
	public:

		ExternExpression(Token token, Expression* name, std::string& ret_type, std::vector<std::pair<std::string, std::string>>* args)
		{
			this->args = args;
			this->name = name;
			this->token = token;
			this->ret_type = ret_type;
		}

		~ExternExpression()
		{
			delete name;

			if (args)
			{
				//for (auto ii : *args)
				//delete ii;
				delete args;
			}
		}

		void SetParent(Expression* parent)
		{
			this->Parent = parent;
			if (name)
				name->SetParent(this);
			//for (auto ii : *args)
			//	ii->SetParent(this);
		}

		CValue Compile(CompilerContext* context);

		void CompileDeclarations(CompilerContext* context);
	};

	class StructExpression : public Expression
	{
		std::string name;
		std::vector<std::pair<std::string, std::string>>* elements;
		Token token;
		std::string ret_type;
	public:

		StructExpression(Token token, std::string& name, std::vector<std::pair<std::string, std::string>>* elements)
		{
			this->elements = elements;
			this->name = name;
			this->token = token;
			this->ret_type = ret_type;
		}

		~StructExpression()
		{
			//delete name;

			if (elements)
			{
				//for (auto ii : *args)
				//delete ii;
				delete elements;
			}
		}

		void SetParent(Expression* parent)
		{
			this->Parent = parent;
			//if (elements)
			//name->SetParent(this);
			//for (auto ii : *args)
			//	ii->SetParent(this);
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
			context->Line(token.line);

			if (right)
				context->Return(right->Compile(context));
			else
				context->parent->builder.CreateRetVoid();

			return CValue();
		}

		void CompileDeclarations(CompilerContext* context) {};
	};

	/*class BreakExpression: public Expression
	{
	public:

	void SetParent(Expression* parent)
	{
	this->Parent = parent;
	}

	void Compile(CompilerContext* context)
	{
	context->Break();
	}
	};*/

	/*class ContinueExpression: public Expression
	{
	public:
	void SetParent(Expression* parent)
	{
	this->Parent = parent;
	}

	void Compile(CompilerContext* context)
	{
	context->Continue();
	}
	};

	class YieldExpression: public Expression
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