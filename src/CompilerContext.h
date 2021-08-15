#ifndef JET_COMPILER_CONTEXT_HEADER
#define JET_COMPILER_CONTEXT_HEADER

#include "Compilation.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

#include <vector>

#include "types/Types.h"
#include "types/Function.h"
#include "Token.h"

#include "llvm/ExecutionEngine/ExecutionEngine.h"
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

		std::vector<CValue> to_destruct;

		std::vector<Scope*> next;
		Scope* prev;

		bool destructed = false;

		void Destruct(CompilerContext* context, llvm::Value* ignore = 0);//the ignore is used for returns
	};

	struct TCScope
	{
		//for doing typecheck
		std::map<std::string, Type*> named_values;
		std::vector<TCScope*> next;
		TCScope* prev;
	};

	//CValue CallFunction(CompilerContext* context, Function* fun, std::vector<llvm::Value*>& argsv, bool devirtualize);

	//compiles functions
	class Compiler;
	class CompilerContext
	{
		friend class FunctionExpression;
		friend class MatchExpression;
		Scope* scope;
		TCScope* tscope;

		std::stack<std::pair<llvm::BasicBlock*, llvm::BasicBlock*>> loops;


		std::vector<CValue> to_destruct;

	public:
		Compilation* root;
		CompilerContext* parent;
		llvm::LLVMContext& context;
		Function* function;

		CompilerContext(Compilation* root, CompilerContext* parent) : context(root->context)
		{
			this->root = root;
			this->parent = parent;
			this->scope = new Scope;
			this->scope->prev = 0;

			this->tscope = new TCScope;
			this->tscope->prev = 0;
		}

		~CompilerContext()
		{
			for (auto ii : this->scope->next)
				delete ii;
			delete this->scope;
		}

		CValue Float(float value)
		{
			return CValue(root->FloatType, llvm::ConstantFP::get(root->context, llvm::APFloat(value)));
		}

		CValue Double(double value)
		{
			return CValue(root->DoubleType, llvm::ConstantFP::get(root->context, llvm::APFloat(value)));
		}

		CValue Integer(int value)
		{
			return CValue(root->IntType, llvm::ConstantInt::get(root->context, llvm::APInt(32, value, true)));
		}

		CValue String(const std::string& str)
		{
			auto fname = llvm::ConstantDataArray::getString(root->context, str, true);

			auto FBloc = new llvm::GlobalVariable(*root->module, fname->getType(), true,
				llvm::GlobalValue::InternalLinkage, fname,
				"const_string");
			auto const_inst32 = llvm::ConstantInt::get(this->context, llvm::APInt(32, 0, false));
			std::vector<llvm::Value*> const_ptr_7_indices = { const_inst32, const_inst32 };
			
			auto res = this->root->builder.CreateGEP(FBloc, const_ptr_7_indices, "x");

			return CValue(this->root->CharPointerType, res);
		}

		void RegisterLocal(const std::string& name, CValue val, bool needs_destruction = false, bool is_const = false);

        void EndStatement();// todo clean this idea up, this calls destructor on anything created in this statement
		std::function<void(const std::string& name, Type* ty)> local_reg_callback;
		void TCRegisterLocal(const std::string& name, Type* ty)
		{
			if (local_reg_callback)
				local_reg_callback(name, ty);
			this->tscope->named_values[name] = ty;
		}

		CValue GetVariable(const std::string& name, bool error = true);

		Type* TCGetVariable(const std::string& name)
		{
			auto cur = this->tscope;
			Type* value = 0;
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

			if (value == 0)
			{
				//ok, now search globals
				auto sym = this->root->GetVariableOrFunction(name);
				if (sym.type != SymbolType::Invalid)
				{
					if (sym.type == SymbolType::Function)// function != 0)
					{
						auto function = sym.fn;
						function->Load(this->root);
						return function->GetType(this->root);
					}
					else if (sym.type == SymbolType::Variable)
					{
						//variable
						return sym.val->type;
					}
				}
				//gotta figure out how to put constants here....
				//throw 7;
				//todo do this
				//auto global = this->root->globals.find(name);
				//if (global != this->root->globals.end())
				//	return global->second.type;// ->second;

				/*auto function = this->root->GetFunction(name);
				if (function != 0)
				{
					function->Load(this->root);
					return function->GetType(this->root);
				}*/

				if (this->function->is_lambda)
				{
					this->root->Error("Lambda checking like this unimplemented", *this->current_token);
					//auto var = this->parent->GetVariable(name);

					//look in locals above me
					/*CValue location = this->Load("_capture_data");
					auto storage_t = this->function->storage_type;

					//append the new type
					std::vector<llvm::Type*> types;
					for (int i = 0; i < this->captures.size(); i++)
						types.push_back(storage_t->getContainedType(i));

					types.push_back(var.type->base->GetLLVMType());
					storage_t = this->function->storage_type = storage_t->create(types);

					auto data = root->builder.CreatePointerCast(location.val, storage_t->getPointerTo());

					//load it, then store it as a local
					auto val = root->builder.CreateGEP(data, { root->builder.getInt32(0), root->builder.getInt32(this->captures.size()) });

					CValue value;
					value.val = root->builder.CreateAlloca(var.type->base->GetLLVMType());
					value.type = var.type;

					this->RegisterLocal(name, value);//need to register it as immutable 

					root->builder.CreateStore(root->builder.CreateLoad(val), value.val);
					this->captures.push_back(name);

					return value;*/
					//return var.type;
				}

				this->root->Error("Undeclared identifier '" + name + "'", *current_token);
			}
			return value;
		}

		void Store(CValue loc, CValue val, bool rvo = false);

		CValue Load(const std::string& name);

		void SetDebugLocation(const Token& t);

		void AddAndSetNamespace(const std::string& name)
		{
			auto res = this->root->ns->members.find(name);
			if (res != this->root->ns->members.end())
			{
				this->root->Error("Namespace '" + name + "' already exists", *this->current_token);
				return;
			}

			//create new one
			auto ns = new Namespace(name, this->root->ns);

			this->root->ns->members.insert({ name, Symbol(ns) });
			this->root->ns = ns;
		}

		void SetNamespace(const std::string& name)
		{
			auto res = this->root->ns->members.find(name);
			if (res != this->root->ns->members.end())
			{
				this->root->ns = res->second.ns;
				return;
			}

			//create new one
			auto ns = new Namespace(name, this->root->ns);

			this->root->ns->members.insert({ name, Symbol(ns) });
			this->root->ns = ns;
		}

		void PopNamespace()
		{
			this->root->ns = this->root->ns->parent;
		}


		CValue UnaryOperation(TokenType operation, CValue value);

		CValue BinaryOperation(Jet::TokenType op, CValue left, CValue lhsptr, CValue right);

		void PushLoop(llvm::BasicBlock* Break, llvm::BasicBlock* Continue)
		{
			loops.push({ Break, Continue });
		}

		void PopLoop()
		{
			loops.pop();
		}

		CValue GetSizeof(Type* t)
		{
			auto null = llvm::ConstantPointerNull::get(t->GetLLVMType()->getPointerTo());
			auto ptr = root->builder.CreateGEP(null, root->builder.getInt32(1));
			ptr = root->builder.CreatePtrToInt(ptr, root->IntType->GetLLVMType(), "sizeof");
			return CValue(root->IntType, ptr);
		}

		void Continue()
		{
			if (loops.empty())
				this->root->Error("Cannot continue from outside loop!", *current_token);

			this->root->builder.CreateBr(loops.top().second);

			llvm::BasicBlock *bb = llvm::BasicBlock::Create(root->context, "post.continue", this->function->f);
			this->root->builder.SetInsertPoint(bb);
		}

		void Break()
		{
			if (loops.empty())
				this->root->Error("Cannot break from outside loop!", *current_token);

			this->root->builder.CreateBr(loops.top().first);

			llvm::BasicBlock *bb = llvm::BasicBlock::Create(root->context, "post.break", this->function->f);
			this->root->builder.SetInsertPoint(bb);
		}

		llvm::ReturnInst* Return(CValue ret);

		const Token* current_token;
		inline void CurrentToken(const Token* token)
		{
			current_token = token;
		}

		TCScope* TCPushScope()
		{
			auto temp = this->tscope;
			this->tscope = new TCScope;
			this->tscope->prev = temp;

			temp->next.push_back(this->tscope);
			return this->tscope;
		}

		void TCPopScope()
		{
			this->tscope = this->tscope->prev;
		}

		Scope* PushScope();

		void PopScope();

		void Construct(CValue value, llvm::Value* size);
		void Destruct(CValue pointer, llvm::Value* arr_size);
        void DestructLater(CValue data);

		CValue DoCast(Type* t, CValue value, bool Explicit = false, llvm::Value* alloca = 0);
		bool CheckCast(Type* src, Type* dest, bool Explicit = false, bool Throw = true);

		CompilerContext* StartFunctionDefinition(Function* function);

		Symbol GetMethod(const std::string& name, const std::vector<Type*>& args, Type* Struct, bool& is_constructor);
		CValue Call(const std::string& name, const std::vector<CValue>& args, Type* Struct = 0, bool devirtualize = false, bool is_const = false);

		std::vector<std::string> captures;
		void WriteCaptures(llvm::Value* lambda);
	};
};
#endif
