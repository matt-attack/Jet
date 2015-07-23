#ifndef JET_COMPILER_CONTEXT_HEADER
#define JET_COMPILER_CONTEXT_HEADER

//#include "Compiler.h"
//#include "Parser.h"
//#include "Lexer.h"

#include <llvm\IR\IRBuilder.h>
#include <llvm\IR\LLVMContext.h>
#include <llvm\IR\Module.h>

//#include <string>
//#include <vector>
//#include <map>

#include "Types.h"
#include "Token.h"

#include "llvm/ExecutionEngine/ExecutionEngine.h"
//#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Transforms/Scalar.h"


//lets get functions with the same name but different args working for structs
//then add templates/generics
namespace Jet
{
	//global compiler context

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

		void RegisterLocal(const std::string& name, CValue val)
		{
			if (this->scope->named_values.find(name) != this->scope->named_values.end())
				this->parent->Error("Variable '" + name + "' already defined", *this->current_token);
			this->scope->named_values[name] = val;
		}

		CValue String(const std::string& str)
		{
			auto fname = llvm::ConstantDataArray::getString(parent->context, str, true);

			auto FBloc = new llvm::GlobalVariable(*parent->module, fname->getType(), true,
				llvm::GlobalValue::InternalLinkage, fname,
				"const_string");
			auto const_inst32 = llvm::ConstantInt::get(llvm::getGlobalContext(), llvm::APInt(32, llvm::StringRef("0"), 10));
			std::vector<llvm::Constant*> const_ptr_7_indices = { const_inst32, const_inst32 };
			auto res = llvm::ConstantExpr::getGetElementPtr(FBloc, const_ptr_7_indices);

			return CValue(this->parent->LookupType("char*"), res);
		}

		CValue GetVariable(const std::string& name)
		{
			auto cur = this->scope;
			CValue value;
			do
			{
				auto iter = cur->named_values.find(name);
				if (iter != cur->named_values.end())
				{
					value = iter->second;
					break;
				}
				cur = cur->prev;
			} while (cur);

			if (value.type->type == Types::Void)
			{
				//ok, now search globals
				auto global = this->parent->globals.find(name);
				if (global != this->parent->globals.end())
					return global->second;

				auto function = this->parent->functions.find(name);
				//function->second->f->getType()->dump();
				if (function != this->parent->functions.end())
				{
					function->second->Load(this->parent);
					return CValue(function->second->GetType(this->parent), function->second->f);
				}

				this->parent->Error("Undeclared identifier '" + name + "'", *current_token);
			}
			return value;
		}

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

		void SetDebugLocation(const Token& t)
		{
			assert(this->function->loaded);
			this->parent->builder.SetCurrentDebugLocation(llvm::DebugLoc::get(t.line, t.column, this->function->scope.get()));
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

		llvm::ReturnInst* Return(CValue ret)
		{
			//try and cast if we can
			if (this->function == 0)
				this->parent->Error("Cannot return from outside function!", *current_token);

			//call destructors
			auto cur = this->scope;
			do
			{
				for (auto ii : cur->named_values)
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
				cur = cur->prev;
			} while (cur);

			if (ret.val)
				ret = this->DoCast(this->function->return_type, ret);
			return parent->builder.CreateRet(ret.val);
		}

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

		CValue DoCast(Type* t, CValue value, bool Explicit = false)
		{
			if (value.type->type == t->type && value.type->data == t->data)
				return value;

			llvm::Type* tt = GetType(t);
			if (value.type->type == Types::Float && t->type == Types::Double)
			{
				//lets do this
				return CValue(t, parent->builder.CreateFPExt(value.val, tt));
			}
			if (value.type->type == Types::Double && t->type == Types::Float)
			{
				//lets do this
				return CValue(t, parent->builder.CreateFPTrunc(value.val, tt));
			}
			if (value.type->type == Types::Double || value.type->type == Types::Float)
			{
				//float to int
				if (t->type == Types::Int || t->type == Types::Short || t->type == Types::Char)
					return CValue(t, parent->builder.CreateFPToSI(value.val, tt));

				//remove me later float to bool
				if (t->type == Types::Bool)
					return CValue(t, parent->builder.CreateFCmpONE(value.val, llvm::ConstantFP::get(llvm::getGlobalContext(), llvm::APFloat(0.0))));
			}
			if (value.type->type == Types::Int || value.type->type == Types::Short || value.type->type == Types::Char)
			{
				//int to float
				if (t->type == Types::Double || t->type == Types::Float)
					return CValue(t, parent->builder.CreateSIToFP(value.val, tt));
				if (t->type == Types::Bool)
					return CValue(t, parent->builder.CreateIsNotNull(value.val));
				if (t->type == Types::Pointer)
				{
					return CValue(t, parent->builder.CreateIntToPtr(value.val, GetType(t)));
				}

				if (value.type->type == Types::Int && (t->type == Types::Char || t->type == Types::Short))
				{
					return CValue(t, parent->builder.CreateTrunc(value.val, GetType(t)));
				}
			}
			if (value.type->type == Types::Pointer)
			{
				//pointer to bool
				if (t->type == Types::Bool)
					return CValue(t, parent->builder.CreateIsNotNull(value.val));
				if (Explicit)
				{
					if (t->type == Types::Pointer)
					{
						//pointer to pointer cast;
						return CValue(t,parent->builder.CreatePointerCast(value.val, GetType(t), "ptr2ptr"));
					}
				}
			}
			if (value.type->type == Types::Array)
			{
				//array to pointer
				if (t->type == Types::Pointer)
				{
					if (t->base == value.type->base)
					{
						//lets just try it
						//fixme later
						//ok, this doesnt work because the value is getting loaded beforehand!!!
						/*std::vector<llvm::Value*> arr = { parent->builder.getInt32(0), parent->builder.getInt32(0) };

						this->f->dump();
						value.val->dump();
						value.val = parent->builder.CreateGEP(value.val, arr, "array2ptr");
						value.val->dump();
						return CValue(t, value.val);*/
					}
				}
			}
			if (value.type->type == Types::Function)
			{
				if (t->type == Types::Function && t->function == value.type->function)
					return value;
			}

			this->parent->Error("Cannot cast '" + value.type->ToString() + "' to '" + t->ToString() + "'!", *current_token);
		}

		CompilerContext* AddFunction(const std::string& fname, Type* ret, const std::vector<std::pair<Type*, std::string>>& args, bool member = false);

		Function* GetMethod(const std::string& name, const std::vector<CValue>& args, Type* Struct = 0);
		CValue Call(const std::string& name, const std::vector<CValue>& args, Type* Struct = 0);
	};
};
#endif