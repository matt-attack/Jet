#ifndef JET_COMPILER_CONTEXT_HEADER
#define JET_COMPILER_CONTEXT_HEADER

//#include "Compiler.h"
//#include "Parser.h"
//#include "Lexer.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

//#include <string>
//#include <vector>
//#include <map>

#include "Types/Types.h"
#include "Token.h"

#include "llvm/ExecutionEngine/ExecutionEngine.h"
//#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Transforms/Scalar.h"

namespace Jet
{
	struct Scope
	{
		std::map<std::string, CValue> named_values;

		std::vector<Scope*> next;
		Scope* prev;
	};

	//compiles functions
	class Compiler;
	class CompilerContext
	{
		Scope* scope;

	public:
		Compilation* parent;

		llvm::Function* f;
		Function* function;

		CompilerContext(Compilation* parent)
		{
			this->parent = parent;
			this->scope = new Scope;
			this->scope->prev = 0;
		}

		~CompilerContext()
		{
			for (auto ii : this->scope->next)
				delete ii;
			delete this->scope;
		}

		CValue Float(double value)
		{
			return CValue(parent->DoubleType, llvm::ConstantFP::get(parent->context, llvm::APFloat(value)));
		}

		CValue Integer(int value)
		{
			return CValue(parent->IntType, llvm::ConstantInt::get(parent->context, llvm::APInt(32, value, true)));
		}

		CValue String(const std::string& str)
		{
			auto fname = llvm::ConstantDataArray::getString(parent->context, str, true);

			auto FBloc = new llvm::GlobalVariable(*parent->module, fname->getType(), true,
				llvm::GlobalValue::InternalLinkage, fname,
				"const_string");
			auto const_inst32 = llvm::ConstantInt::get(llvm::getGlobalContext(), llvm::APInt(32, llvm::StringRef("0"), 10));
			std::vector<llvm::Value*> const_ptr_7_indices = { const_inst32, const_inst32 };
			auto type = FBloc->getType()->getElementType()->getArrayElementType()->getPointerTo();
			//FBloc->getType()->getElementType()->getArrayElementType()->getPointerTo()->dump();

			auto res = this->parent->builder.CreateGEP(FBloc, const_ptr_7_indices, "x");
			//auto res = llvm::ConstantExpr::getGetElementPtr(FBloc, const_ptr_7_indices, "x");
			//fname->get
			return CValue(this->parent->LookupType("char*"), res);
		}

		void RegisterLocal(const std::string& name, CValue val)
		{
			if (this->scope->named_values.find(name) != this->scope->named_values.end())
				this->parent->Error("Variable '" + name + "' already defined", *this->current_token);
			this->scope->named_values[name] = val;
		}

		CValue GetVariable(const std::string& name);

		llvm::StoreInst* Store(const std::string& name, CValue val)
		{
			//for each scope
			CValue value = GetVariable(name);

			val = this->DoCast(value.type->base, val);

			return parent->builder.CreateStore(val.val, value.val);
		}

		CValue Load(const std::string& name)
		{
			CValue value = GetVariable(name);
			if (value.type->type == Types::Function)
				return value;
			
			return CValue(value.type->base, parent->builder.CreateLoad(value.val, name.c_str()));
		}

		void SetDebugLocation(const Token& t);

		void SetNamespace(const std::string& name)
		{
			auto res = this->parent->ns->members.find(name);
			if (res != this->parent->ns->members.end())
			{
				this->parent->ns = res->second.ns;
				return;
			}

			//create new one
			auto ns = new Namespace;
			ns->name = name;
			ns->parent = this->parent->ns;

			this->parent->ns->members.insert({ name, Symbol(ns) });
			this->parent->ns = ns;
		}

		void PopNamespace()
		{
			this->parent->ns = this->parent->ns->parent;
		}


		CValue UnaryOperation(TokenType operation, CValue value);

		CValue BinaryOperation(Jet::TokenType op, CValue left, CValue right);

		std::stack<std::pair<llvm::BasicBlock*, llvm::BasicBlock*>> loops;
		void PushLoop(llvm::BasicBlock* Break, llvm::BasicBlock* Continue)
		{
			loops.push({ Break, Continue });
		}

		void PopLoop()
		{
			loops.pop();
		}

		void Continue()
		{
			if (loops.empty())
				this->parent->Error("Cannot continue from outside loop!", *current_token);

			this->parent->builder.CreateBr(loops.top().second);

			llvm::BasicBlock *bb = llvm::BasicBlock::Create(parent->context, "post.continue", this->f);
			this->parent->builder.SetInsertPoint(bb);
		}

		void Break()
		{
			if (loops.empty())
				this->parent->Error("Cannot break from outside loop!", *current_token);

			this->parent->builder.CreateBr(loops.top().first);

			llvm::BasicBlock *bb = llvm::BasicBlock::Create(parent->context, "post.break", this->f);
			this->parent->builder.SetInsertPoint(bb);
		}

		llvm::ReturnInst* Return(CValue ret);

		Token* current_token;
		inline void CurrentToken(Token* token)
		{
			current_token = token;
		}

		Scope* PushScope()
		{
			auto temp = this->scope;
			this->scope = new Scope;
			this->scope->prev = temp;

			temp->next.push_back(this->scope);
			return this->scope;
		}

		void PopScope()
		{
			//call destructors
			if (this->scope->prev != 0 && this->scope->prev->prev != 0)
			{
				for (auto ii : this->scope->named_values)
				{
					if (ii.second.type->type == Types::Struct)
					{
						//look for destructor
						auto name = "~" + (ii.second.type->data->template_base ? ii.second.type->data->template_base->name : ii.second.type->data->name);
						auto destructor = ii.second.type->data->functions.find(name);
						if (destructor != ii.second.type->data->functions.end())
						{
							//call it
							this->Call(name, { CValue(this->parent->LookupType(ii.second.type->ToString() + "*"), ii.second.val) }, ii.second.type);
						}
					}
				}
			}

			auto temp = this->scope;
			this->scope = this->scope->prev;
		}

		CValue DoCast(Type* t, CValue value, bool Explicit = false);

		CompilerContext* AddFunction(const std::string& fname, Type* ret, const std::vector<std::pair<Type*, std::string>>& args, bool member = false);

		Function* GetMethod(const std::string& name, const std::vector<CValue>& args, Type* Struct = 0);
		CValue Call(const std::string& name, const std::vector<CValue>& args, Type* Struct = 0);
	};
};
#endif