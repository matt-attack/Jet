#ifndef _CONTROL_EXPRESSIONS_HEADER
#define _CONTROL_EXPRESSIONS_HEADER

#include <string>
#include <stdio.h>
#include <vector>

#include "Compiler.h"
#include "CompilerContext.h"
#include "ExpressionVisitor.h"
#include "Source.h"
#include "types/Function.h"
#include "Expressions.h"

namespace Jet
{
	class Source;
	class Compiler;


	class CallExpression : public Expression
	{
		Token open, close;

	public:
		Expression* left;
		std::vector<std::pair<Expression*, Token>>* args;
		friend class FunctionParselet;
		CallExpression(Token open, Token close, Expression* left, std::vector<std::pair<Expression*, Token>>* args)
		{
			this->open = open;
			this->close = close;
			this->left = left;
			this->args = args;
		}

		~CallExpression()
		{
			delete this->left;
			if (args)
			{
				for (auto ii : *args)
					delete ii.first;

				delete args;
			}
		}

		void SetParent(Expression* parent)
		{
			this->parent = parent;
			left->SetParent(this);
			for (auto ii : *args)
				if (ii.first)
					ii.first->SetParent(this);
		}

		Function* call; //store the function being called here so we dont have to look up again (if we know it
		virtual Type* TypeCheck(CompilerContext* context);

		CValue Compile(CompilerContext* context);

		Type* GetReturnType()
		{
			return 0;
		}

		void CompileDeclarations(CompilerContext* context) {};

		void Print(std::string& output, Source* source)
		{
			this->left->Print(output, source);

			open.Print(output, source);

			for (auto ii : *this->args)
			{
				ii.first->Print(output, source);

				if (ii.second.text.length())
					ii.second.Print(output, source);
			}
			this->close.Print(output, source);
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);
			left->Visit(visitor);
			if (this->args)
			{
				for (auto ii : *this->args)
					ii.first->Visit(visitor);
			}
		}

        std::pair<Token, Token> GetTokenRange() override
        {
            return { open, close };
        }
	};


	struct MatchCase
	{
		Token type, name, pointy;
		BlockExpression* block;
	};
	class MatchExpression : public Expression
	{
		Token token, open, close, open_brace, close_brace;
		Expression* var;

		std::vector<MatchCase> cases;
	public:

		MatchExpression(Token token, Token open, Token close, Expression* thing, Token ob, std::vector<MatchCase>&& elements, Token cb)
		{
			this->open_brace = ob;
			this->close_brace = cb;
			this->open = open;
			this->close = close;
			this->token = token;
			this->cases = elements;
			this->var = thing;
		}

		CValue Compile(CompilerContext* context);

		void CompileDeclarations(CompilerContext* context)
		{

		};

		void Print(std::string& output, Source* source)
		{
			token.Print(output, source);
			open.Print(output, source);
			var->Print(output, source);
			close.Print(output, source);

			open_brace.Print(output, source);
			for (auto ii : cases)
			{
				ii.type.Print(output, source);
				if (ii.type.type != TokenType::Default)
					ii.name.Print(output, source);
				ii.pointy.Print(output, source);
				ii.block->Print(output, source);
			}
			close_brace.Print(output, source);
			//token.Print(output, source);
		}

		virtual Type* TypeCheck(CompilerContext* context)
		{
			//build the type
			//get the type fixme later
			return 0;
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);
            var->Visit(visitor);

            for (auto& ii: cases)
            {
                ii.block->Visit(visitor);
            }
		}

        std::pair<Token, Token> GetTokenRange() override
        {
            return { token, close_brace };
        }
	};

	class WhileExpression : public Expression
	{
		Expression* condition;
		ScopeExpression* block;
		Token token, open_bracket, close_bracket;
	public:

		WhileExpression(Token token, Token ob, Expression* cond, Token cb, ScopeExpression* block)
		{
			this->open_bracket = ob;
			this->close_bracket = cb;
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
			this->parent = parent;
			block->SetParent(this);
			condition->SetParent(this);
		}

		virtual Type* TypeCheck(CompilerContext* context)
		{
			//check that type of top can be converted to bool
			auto ct = condition->TypeCheck(context);
			//if (ct->type != Types::Bool)
			{
				//throw 7;//need to check if it can be converted
			}
			block->TypeCheck(context);
			return 0;
		}

		CValue Compile(CompilerContext* context)
		{
			context->CurrentToken(&token);

			llvm::BasicBlock *start = llvm::BasicBlock::Create(context->context, "whilestart");
			llvm::BasicBlock *body = llvm::BasicBlock::Create(context->context, "whilebody");
			llvm::BasicBlock *end = llvm::BasicBlock::Create(context->context, "whileend");


			context->root->builder.CreateBr(start);
			context->function->f_->getBasicBlockList().push_back(start);
			context->root->builder.SetInsertPoint(start);

			auto cond = this->condition->Compile(context);
			cond = context->DoCast(context->root->BoolType, cond);
			context->root->builder.CreateCondBr(cond.val, body, end);


			context->function->f_->getBasicBlockList().push_back(body);
			context->root->builder.SetInsertPoint(body);

			context->PushLoop(end, start);
			this->block->Compile(context);
			context->PopLoop();

			context->root->builder.CreateBr(start);

			context->function->f_->getBasicBlockList().push_back(end);
			context->root->builder.SetInsertPoint(end);

			return CValue(context->root->VoidType, 0);
		}

		void CompileDeclarations(CompilerContext* context) {};

		void Print(std::string& output, Source* source)
		{
			//add tokens for the ( )
			token.Print(output, source);
			open_bracket.Print(output, source);// output += "("; fixme
			this->condition->Print(output, source);
			close_bracket.Print(output, source);// output += ")";

			this->block->Print(output, source);
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);
			condition->Visit(visitor);
			block->Visit(visitor);
		}

        std::pair<Token, Token> GetTokenRange() override
        {
            return { token, block->GetTokenRange().second };
        }
	};

	class ForExpression : public Expression
	{
		Expression* condition, *initial, *incr;
		ScopeExpression* block;
		Token token, open_bracket, semicolon1, semicolon2, close_bracket;
	public:
		ForExpression(Token token, Token ob, Expression* init, Token s1, Expression* cond, Token s2, Expression* incr, Token cb, ScopeExpression* block)
		{
			this->condition = cond;
			this->open_bracket = ob;
			this->close_bracket = cb;
			this->semicolon1 = s1;
			this->semicolon2 = s2;
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
			this->parent = parent;
			block->SetParent(this);
			if (incr)
				incr->SetParent(block);
			if (condition)
				condition->SetParent(this);
			if (initial)
				initial->SetParent(block);
		}

		virtual Type* TypeCheck(CompilerContext* context)
		{
			//check type of middle to be bool or nothing
			//then just check others
			if (initial)
				initial->TypeCheck(context);

			if (condition)
				auto ty = condition->TypeCheck(context);
			//if (ty && cant convert to bool)
			//error
			//incr->TypeCheck(context);
			block->TypeCheck(context);
			//now crashing here
			//throw 7;
			return 0;
		}

		CValue Compile(CompilerContext* context)
		{
			context->CurrentToken(&token);

			context->PushScope();

			if (this->initial)
				this->initial->Compile(context);

			llvm::BasicBlock *start = llvm::BasicBlock::Create(context->context, "forstart");
			llvm::BasicBlock *body = llvm::BasicBlock::Create(context->context, "forbody");
			llvm::BasicBlock *end = llvm::BasicBlock::Create(context->context, "forend");
			llvm::BasicBlock *cont = llvm::BasicBlock::Create(context->context, "forcontinue");

            auto f = context->function->f_;

			//insert stupid branch
			context->root->builder.CreateBr(start);
			f->getBasicBlockList().push_back(start);
			context->root->builder.SetInsertPoint(start);

			if (this->condition)
			{
				auto cond = this->condition->Compile(context);
				cond = context->DoCast(context->root->BoolType, cond);

				context->root->builder.CreateCondBr(cond.val, body, end);
			}
			else
			{
				context->root->builder.CreateBr(body);
			}

			f->getBasicBlockList().push_back(body);
			context->root->builder.SetInsertPoint(body);

			context->PushLoop(end, cont);
			this->block->Compile(context);
			context->PopLoop();

			context->root->builder.CreateBr(cont);

			//insert continue branch here
			f->getBasicBlockList().push_back(cont);
			context->root->builder.SetInsertPoint(cont);

			if (incr)
				this->incr->Compile(context);

			context->root->builder.CreateBr(start);

			f->getBasicBlockList().push_back(end);
			context->root->builder.SetInsertPoint(end);

			context->PopScope();

			return CValue(context->root->VoidType, 0);
		}

		void CompileDeclarations(CompilerContext* context) {};

		void Print(std::string& output, Source* source)
		{
			//add tokens for the ( )
			token.Print(output, source);
			open_bracket.Print(output, source);// output += "("; fixme
			if (this->initial)
				this->initial->Print(output, source);
			semicolon1.Print(output, source);// output += ";";
			if (this->condition)
				this->condition->Print(output, source);
			semicolon2.Print(output, source);// output += ";";
			if (this->incr)
				this->incr->Print(output, source);
			close_bracket.Print(output, source);// output += ")";

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

        std::pair<Token, Token> GetTokenRange() override
        {
            return { token, block->GetTokenRange().second };
        }
	};

	class CaseExpression : public Expression
	{
		Token token;
	public:
		Token value, colon;
		CaseExpression(Token token, Token value, Token colon)
		{
			this->colon = colon;
			this->value = value;
			this->token = token;
		}

		~CaseExpression()
		{
		}

		virtual void SetParent(Expression* parent)
		{
			this->parent = parent;
		}

		CValue Compile(CompilerContext* context);

		void CompileDeclarations(CompilerContext* context) {};

		void Print(std::string& output, Source* source)
		{
			//add tokens for the ( )
			token.Print(output, source);

			value.Print(output, source);

			colon.Print(output, source);
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

        std::pair<Token, Token> GetTokenRange() override
        {
            return { token, colon };
        }
	};

	class DefaultExpression : public Expression
	{
		Token token, colon;
	public:
		DefaultExpression(Token token, Token colon)
		{
			this->colon = colon;
			this->token = token;
		}

		~DefaultExpression()
		{
		}

		virtual void SetParent(Expression* parent)
		{
			this->parent = parent;
		}

		CValue Compile(CompilerContext* context);

		void CompileDeclarations(CompilerContext* context) {};

		virtual Type* TypeCheck(CompilerContext* context)
		{
			throw 7;

			return 0;
		}

		void Print(std::string& output, Source* source)
		{
			//add tokens for the ( )
			token.Print(output, source);
			colon.Print(output, source);
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);
		}

        std::pair<Token, Token> GetTokenRange() override
        {
            return { token, colon };
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

		Token open_bracket, close_bracket;

		bool first_case;
		SwitchExpression(Token token, Token ob, Expression* var, Token cb, BlockExpression* block)
		{
			this->open_bracket = ob;
			this->close_bracket = cb;
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

		virtual Type* TypeCheck(CompilerContext* context)
		{
			throw 7;

			return 0;
		}

		virtual void SetParent(Expression* parent)
		{
			this->parent = parent;
			this->var->SetParent(this);
			this->block->SetParent(this);
		}

		CValue Compile(CompilerContext* context);

		void CompileDeclarations(CompilerContext* context) {};

		void Print(std::string& output, Source* source)
		{
			//add tokens for the ( )
			token.Print(output, source);
			open_bracket.Print(output, source);// output += "("; fixme
			var->Print(output, source);
			close_bracket.Print(output, source);// output += ")";

			this->block->Print(output, source);
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);
			var->Visit(visitor);
			block->Visit(visitor);
		}

        std::pair<Token, Token> GetTokenRange() override
        {
            return { token, block->GetTokenRange().second };
        }
	};

	struct Branch
	{
		Token token, open_bracket, close_bracket;
		BlockExpression* block;
		Expression* condition;

		Branch(Token token, Token ob, Token cb, BlockExpression* block, Expression* condition)
		{
			this->open_bracket = ob;
			this->close_bracket = cb;
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

		virtual Type* TypeCheck(CompilerContext* context)
		{
			for (auto ii : this->branches)
			{
				auto ct = ii->condition->TypeCheck(context);
				//make sure this is a bool
				ii->block->TypeCheck(context);
			}

			if (this->Else)
				this->Else->block->TypeCheck(context);
			return 0;
		}

		virtual void SetParent(Expression* parent)
		{
			this->parent = parent;
			if (this->Else)
				this->Else->block->SetParent(this);
			for (auto& ii : branches)
			{
				ii->block->SetParent(this);
				ii->condition->SetParent(this);
			}
		}

		CValue Compile(CompilerContext* context);

		void CompileDeclarations(CompilerContext* context) {};

		void Print(std::string& output, Source* source)
		{
			//this->token.Print(output, source);

			for (auto ii : this->branches)
			{
				ii->token.Print(output, source);
				ii->open_bracket.Print(output, source);// output += " (";
				ii->condition->Print(output, source);
				ii->close_bracket.Print(output, source);// output += ")"; fixme
				ii->block->Print(output, source);
			}
			if (this->Else)
			{
				this->Else->token.Print(output, source);

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

        std::pair<Token, Token> GetTokenRange() override
        {
            Token end;
            if (Else)
            {
                end = Else->block->GetTokenRange().second;
            }
            else
            {
                end = branches.back()->block->GetTokenRange().second;
            }
            return { token, end };
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
			this->parent = parent;
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
				context->Return(CValue(context->root->VoidType, 0));// root->builder.CreateRetVoid(); um, im not destructing if I return void

			return CValue(context->root->VoidType, 0);
		}

		virtual Type* TypeCheck(CompilerContext* context)
		{
			//check that return type matches return type of the function
			if (right)
			{
				auto type = right->TypeCheck(context); 
				context->CurrentToken(&this->token);
				context->CheckCast(type, context->function->return_type_, false, true);
			}

			context->function->has_return_ = true;

			return 0;
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

        std::pair<Token, Token> GetTokenRange() override
        {
            return { token, Token() };
        }
	};

	class BreakExpression : public Expression
	{
		Token token;
	public:
		BreakExpression(Token token) : token(token) {}

		void SetParent(Expression* parent)
		{
			this->parent = parent;
		}

		CValue Compile(CompilerContext* context)
		{
			context->CurrentToken(&token);
			context->SetDebugLocation(token);
			context->Break();

			return CValue(context->root->VoidType, 0);
		}

		virtual Type* TypeCheck(CompilerContext* context)
		{
			return 0;
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

        std::pair<Token, Token> GetTokenRange() override
        {
            return { token, Token() };
        }
	};

	class ContinueExpression : public Expression
	{
		Token token;
	public:
		ContinueExpression(Token token) : token(token) {}

		void SetParent(Expression* parent)
		{
			this->parent = parent;
		}

		CValue Compile(CompilerContext* context)
		{
			context->CurrentToken(&token);
			context->SetDebugLocation(token);
			context->Continue();

			return CValue(context->root->VoidType, 0);
		}

		virtual Type* TypeCheck(CompilerContext* context)
		{
			return 0;
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

        std::pair<Token, Token> GetTokenRange() override
        {
            return { token, Token() };
        }
	};

	class YieldExpression : public Expression
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
			this->parent = parent;
			if (right)
				right->SetParent(this);
		}

		virtual Type* TypeCheck(CompilerContext* context)
		{
			//todo
			//throw 7;
			//todo
			return 0;
		}

		void Print(std::string& output, Source* source)
		{
			token.Print(output, source);

			if (this->right)
				this->right->Print(output, source);
			//new_type.Print(output, source);
			//equals.Print(output, source);
			//other_type.Print(output, source);
		}

		void CompileDeclarations(CompilerContext* context)
		{

		}

		CValue Compile(CompilerContext* context);

		virtual void Visit(ExpressionVisitor* visitor)
		{
			throw 7;//todo
			//visitor->Visit(this);
			right->Visit(visitor);
		}

        std::pair<Token, Token> GetTokenRange() override
        {
            return { token, Token() };
        }
	};
}
#endif
