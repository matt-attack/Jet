#ifndef _EXPRESSIONS_HEADER
#define _EXPRESSIONS_HEADER

#include <string>
#include <stdio.h>
#include <vector>

#include "Compiler.h"
#include "CompilerContext.h"
#include "ExpressionVisitor.h"
#include "Source.h"
#include "types/Function.h"

#include <expressions/Expression.h>


namespace Jet
{
	class Source;
	class Compiler;

	class NameExpression : public Expression, public IStorableExpression
	{

	public:
        bool namespaced;
		Token token;
		std::vector<Token>* templates;

		NameExpression(Token name, bool namespaced)
		{
			this->token = name;
			this->templates = 0;
            this->namespaced = namespaced;
		}

		NameExpression(Token name, std::vector<Token>* templates, bool namespaced)
		{
			this->token = name;
			this->templates = templates;
            this->namespaced = namespaced;
		}

		std::string GetName() const
		{
			if (this->templates)
			{
				std::string str = token.text;
				str += "<";

				bool first = true;
				for (auto ii : *this->templates)
				{
					if (first)
						first = false;
					else
					{
						str += ',';
					}
					str += ii.text;
				}
				
				str += ">";
				return str;
			}
			return token.text;
		}

		CValue Compile(CompilerContext* context) override;

		void CompileStore(CompilerContext* context, CValue right) override
		{
			//need to do cast if necessary
			context->CurrentToken(&token);

			//for each scope
			CValue dest = context->GetVariable(token.text);

			if (dest.is_const || !dest.pointer)
			{
				context->root->Error("Cannot assign to const variable '" + BOLD(token.text) + "'", token);
			}

            // convert the dest to a pointer
			context->Store(CValue(dest.type->GetPointerType(), dest.pointer), right);
		}

		void CompileDeclarations(CompilerContext* context) override {};

		Type* TypeCheck(CompilerContext* context) override
		{
			context->CurrentToken(&this->token);
			//this implementation is wrong
			auto var = context->TCGetVariable(token.text);
			if (var->type != Types::Pointer)
				return var;//probably not the right way to handle this, but oh well
			return var->base;
			//lookup type
		}

		void Print(std::string& output, Source* source) override
		{
			token.Print(output, source);
			if (this->templates)
			{
				//todo: free memory
				for (auto ii : *this->templates)
				{
					ii.Print(output, source);
				}
			}
		}

		void Visit(ExpressionVisitor* visitor) override
		{
			visitor->Visit(this);
		}

        std::pair<const Token*, const Token*> GetTokenRange() const override
        {
            return { &token, 0 };
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

			auto rval = right->Compile(context);

			context->PopNamespace();

			return rval;// CValue();
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

        std::pair<const Token*, const Token*> GetTokenRange() const override
        {
            return { &token, 0 };
        }
	};

	class NewExpression : public Expression
	{

	public:
		Token token_, type_;
		Token open_bracket_, close_bracket_;
		Expression* size_;
		std::vector<std::pair<Expression*, Token>>* args_;
		NewExpression(Token tok, Token type, Expression* size, std::vector<std::pair<Expression*, Token>>* args = 0)
		{
			token_ = tok;
			type_ = type;
			args_ = args;
			size_ = size;
		}

		~NewExpression()
		{
			if (args_)
			{
				for (auto ii : *args_)
					delete ii.first;
				delete args_;
			}
		}

		CValue Compile(CompilerContext* context);

		void CompileDeclarations(CompilerContext* context) {};

		virtual Type* TypeCheck(CompilerContext* context)
		{
			auto ty = context->root->LookupType(type_.text);
			return ty->GetPointerType();
		}

		void Print(std::string& output, Source* source)
		{
			token_.Print(output, source);
			type_.Print(output, source);

			if (size_)
			{
				open_bracket_.Print(output, source);
				size_->Print(output, source);
				close_bracket_.Print(output, source);
			}
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);
            if (size_)
            {
                size_->Visit(visitor);
            }
		}

        std::pair<const Token*, const Token*> GetTokenRange() const override
        {
            const Token* end = size_ ? &close_bracket_ : &type_;
            return { &token_, end };
        }
	};

	class FreeExpression : public Expression
	{

	public:
		Token token;
		Token open_bracket, close_bracket;
		Expression* pointer;

		FreeExpression(Token tok, Expression* pointer)
		{
			this->token = tok;
			this->pointer = pointer;
		}

		CValue Compile(CompilerContext* context);

		void CompileDeclarations(CompilerContext* context) {};

		virtual Type* TypeCheck(CompilerContext* context)
		{
			return 0;// context->root->ty->GetPointerType();
		}

		void Print(std::string& output, Source* source)
		{
			token.Print(output, source);
			
			if (this->open_bracket.text.size())
			{
				open_bracket.Print(output, source);// output += '['; fix this
				close_bracket.Print(output, source);// output += ']'; fix this
			}
			pointer->Print(output, source);
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);
		}

        std::pair<const Token*, const Token*> GetTokenRange() const override
        {
            return { &token, pointer->GetTokenRange().second };
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

		struct Number
		{
			enum type
			{
				Float,
				Double,
				Int
			};
			type type;
			union
			{
				double d;
				float f;
				long long int i;
			} data;
		};

		Number GetValue();

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

        std::pair<const Token*, const Token*> GetTokenRange() const override
        {
            return { &token, 0 };
        }
	};

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

		void CompileDeclarations(CompilerContext* context) override {};

		void Print(std::string& output, Source* source) override
		{
			auto code = token.text_ptr;
			auto trivia = token.text_ptr - token.trivia_length;
			for (int i = 0; i < token.trivia_length; i++)
				output += trivia[i];

			output += "\"";
			//fixme!
			auto cur = token.text_ptr + 1;//&source->GetLinePointer(token.line)[token.column]/* token.text_ptr*/ + 1;
			do
			{
				if (*cur == '\"' && *(cur - 1) != '\\')
					break;
				output += *cur;
			} while (*cur++ != 0);

			output += "\"";
		}

		Type* TypeCheck(CompilerContext* context) override
		{
			return context->root->LookupType("char*");
		}

		void Visit(ExpressionVisitor* visitor) override
		{
			visitor->Visit(this);
		}

        std::pair<const Token*, const Token*> GetTokenRange() const override
        {
            return { &token, 0 };
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

        // Returns a CValue with either the pointer or value of the element.
		CValue GetElement(CompilerContext* context, bool for_store = false);
		CValue GetBaseElement(CompilerContext* context);

		Type* GetType(CompilerContext* context, bool tc = false);
		Type* GetBaseType(CompilerContext* context, bool tc = false);
		Type* GetBaseType(Compilation* compiler);

		virtual CValue Compile(CompilerContext* context);
		virtual void CompileStore(CompilerContext* context, CValue right);
		virtual void CompileDeclarations(CompilerContext* context) {};

		virtual void Print(std::string& output, Source* source)
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
            context->CurrentToken(&this->token);
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

        std::pair<const Token*, const Token*> GetTokenRange() const override
        {
            return { &token, &close_bracket };
        }
	};

	class SliceExpression : public Expression
	{

	public:
		Expression* index;
        Expression* length;
		Expression* left;
		Token token, colon, close_bracket;

		SliceExpression(Expression* left, Token t, Expression* index, Token colon, Expression* length, Token cb)
		{
			this->token = t;
			this->left = left;
			this->index = index;
            this->colon = colon;
            this->length = length;
			this->close_bracket = cb;
		}

		~SliceExpression()
		{
			delete left;
			delete index;
            delete length;
		}

		virtual CValue Compile(CompilerContext* context);
		virtual void CompileDeclarations(CompilerContext* context) {};

		virtual void Print(std::string& output, Source* source)
		{
			this->left->Print(output, source);
			token.Print(output, source);
            if (index)
                index->Print(output, source);
            colon.Print(output, source);
            if (length)
                length->Print(output, source);
			/*if (token.type == TokenType::Dot || token.type == TokenType::Pointy)
				member.Print(output, source);
			else if (token.type == TokenType::LeftBracket)
			{
				index->Print(output, source);
				close_bracket.Print(output, source);// output += ']'; fix me!
			}*/
		}

		virtual Type* TypeCheck(CompilerContext* context)
		{
			//auto loc = this->GetElementPointer(context);
			/*context->current_token = &this->token;
			auto type = this->GetType(context, true);
			if (type->type == Types::Function)
				return type;
			return type;// ->base;*/
            // todo
            return 0;
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);

			if (index)
				index->Visit(visitor);
			if (left)
				left->Visit(visitor);
			if (length)
				length->Visit(visitor);
		}

        std::pair<const Token*, const Token*> GetTokenRange() const override
        {
            return { &token, &close_bracket };
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
			context->CurrentToken(&this->token);
			context->CheckCast(tr, tl, false, true);

			return tl;
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);

			left->Visit(visitor);
			right->Visit(visitor);
		}

        std::pair<const Token*, const Token*> GetTokenRange() const override
        {
            return { &token, 0 };
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

        std::pair<const Token*, const Token*> GetTokenRange() const override
        {
            return { &token, 0 };
        }
	};

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

        std::pair<const Token*, const Token*> GetTokenRange() const override
        {
            return { &begin, &end };
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

		void CompileStore(CompilerContext* context, CValue right);

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

        std::pair<const Token*, const Token*> GetTokenRange() const override
        {
            return { &_operator, 0 };
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
			return left;//this is OK for now
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

        std::pair<const Token*, const Token*> GetTokenRange() const override
        {
            return { &_operator, 0 };
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

        std::pair<const Token*, const Token*> GetTokenRange() const override
        {
            auto last = right->GetTokenRange();
            return { left->GetTokenRange().first, last.second ? last.second : last.first };
        }
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
                    context->EndStatement();// destroy anything created in this statement but unused
				}
				catch (int i)
				{

				}
			}

			return CValue(context->root->VoidType, 0);
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
			{
				try
				{
					ii->TypeCheck(context);
				}
				catch (...)
				{

				}
			}
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

        std::pair<const Token*, const Token*> GetTokenRange() const override
        {
            return { &start, &end };
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
			return context->root->IntType;
		}

        std::pair<const Token*, const Token*> GetTokenRange() const override
        {
            return { &token, &end };
        }
	};

	class TypeofExpression : public Expression
	{
		Token begin, end;
		Expression* arg;
		Token token;
	public:
		TypeofExpression(Token token, Token begin, Expression* arg, Token end)
		{
			this->arg = arg;
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
			arg->Print(output, source);
			end.Print(output, source);
		}

		virtual void Visit(ExpressionVisitor* visitor)
		{
			visitor->Visit(this);
		}

		virtual Type* TypeCheck(CompilerContext* context)
		{
			return context->root->IntType;
		}

        std::pair<const Token*, const Token*> GetTokenRange() const override
        {
            return { &token, &end };
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

		Expression* GetInside()
		{
			return this->expr;
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

        std::pair<const Token*, const Token*> GetTokenRange() const override
        {
            return { &begin, &end };
        }
	};
}
#endif
