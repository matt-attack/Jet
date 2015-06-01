#ifndef JET_COMPILER_HEADER
#define JET_COMPILER_HEADER

/*#ifdef _DEBUG
#ifndef DBG_NEW
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#define new DBG_NEW
#endif

#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif*/

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



namespace Jet
{
	void Error(const std::string& msg, Token token);
	void ParserError(const std::string& msg, Token token);

	class BlockExpression;

	struct CValue
	{
		//my type info
		Type* type;
		llvm::Value* val;

		CValue()
		{
			type = &VoidType;
			val = 0;
		}

		CValue(Type* type, llvm::Value* val) : type(type), val(val) {}
	};

	//add global variables
	//global compiler context
	class CompilerContext;
	class Compiler
	{

	public:
		llvm::IRBuilder<> builder;
		llvm::LLVMContext& context;
		llvm::Module* module;
		std::map<std::string, Function*> functions;
		CompilerContext* current_function;

		Compiler() : builder(llvm::getGlobalContext()), context(llvm::getGlobalContext())
		{
			//insert basic types
			types["double"] = new Type(Types::Double);
			types["float"] = &DoubleType;// new Type(Types::Float);
			types["int"] = &IntType;// new Type(Types::Int);
			types["short"] = new Type(Types::Short);
			types["char"] = new Type(Types::Char);
			types["bool"] = &BoolType;// new Type(Types::Bool);
			types["void"] = &VoidType;// new Type(Types::Void);
		}

		void Compile(const char* projectfile);

		void Compile(const char* code, const char* filename);

		void Optimize()
		{
			llvm::legacy::FunctionPassManager OurFPM(module);
			// Set up the optimizer pipeline.  Start with registering info about how the
			// target lays out data structures.
			//TheModule->setDataLayout(*TheExecutionEngine->getDataLayout());
			// Provide basic AliasAnalysis support for GVN.
			OurFPM.add(llvm::createBasicAliasAnalysisPass());
			// Do simple "peephole" optimizations and bit-twiddling optzns.
			OurFPM.add(llvm::createInstructionCombiningPass());
			// Reassociate expressions.
			OurFPM.add(llvm::createReassociatePass());
			// Eliminate Common SubExpressions.
			OurFPM.add(llvm::createGVNPass());
			// Simplify the control flow graph (deleting unreachable blocks, etc).
			OurFPM.add(llvm::createCFGSimplificationPass());

			OurFPM.doInitialization();

			for (auto ii : this->functions)
			{
				if (ii.second->f)
					OurFPM.run(*ii.second->f);
			}
		}

		void OutputIR(const char* filename);
		void OutputPackage();

		void Dump()
		{
			if (module)
				module->dump();
		}

		//get array types inside of structs working
		Type* AdvanceTypeLookup(const std::string& name)
		{
			auto type = types.find(name);
			if (type == types.end())
			{
				//create it, its not a basic type, todo later
				if (name[name.length() - 1] == '*')
				{
					//its a pointer
					auto t = types[name.substr(0, name.length() - 1)];

					Type* type = new Type;
					type->base = t;
					type->type = Types::Pointer;

					types[name] = type;
					return type;
				}
				else if (name[name.length() - 1] == ']')
				{
					//its an array
					int p = 0;
					for (p = 0; p < name.length(); p++)
						if (name[p] == '[')
							break;

					auto len = name.substr(p + 1, name.length() - p - 2);

					auto tname = name.substr(0, p);
					auto t = this->LookupType(tname);

					Type* type = new Type;
					type->base = t;
					type->type = Types::Array;
					type->size = std::stoi(len);//cheat for now
					types[name] = type;
					return type;
				}

				//who knows what type it is, create a dummy one
				Type* type = new Type;
				type->type = Types::Invalid;
				type->data = 0;
				types[name] = type;

				return type;
			}
			return type->second;
		}

		std::map<std::string, Type*> types;
		Type* LookupType(const std::string& name)
		{
			auto type = types[name];
			if (type == 0)
			{
				//time to handle pointers yo
				if (name[name.length() - 1] == '*')
				{
					//its a pointer
					auto t = this->LookupType(name.substr(0, name.length() - 1));

					type = new Type;
					type->base = t;
					type->type = Types::Pointer;

					types[name] = type;
				}
				else if (name[name.length() - 1] == ']')
				{
					//its an array
					int p = 0;
					for (p = 0; p < name.length(); p++)
						if (name[p] == '[')
							break;

					auto len = name.substr(p + 1, name.length() - p - 2);

					auto tname = name.substr(0, p);
					auto t = this->LookupType(tname);

					type = new Type;
					type->base = t;
					type->type = Types::Array;
					type->size = std::stoi(len);//cheat for now
					types[name] = type;
				}
				else
				{
					printf("Error: Couldn't Find Type: %s\n", name.c_str());
					throw 7;
				}
			}

			//load it if it hasnt been loaded
			if (type->loaded == false)
			{
				type->Load(this);
				type->loaded = true;
			}

			return type;
		}
	};

	struct Scope
	{
		std::map<std::string, CValue> named_values;
		Scope* next;
		Scope* prev;
	};

	//compiles functions
	class CompilerContext
	{

	public:
		Scope* scope;
		Compiler* parent;

		llvm::Function* f;

		//std::map<std::string, CValue> named_values;

		Function* function;

		CompilerContext(Compiler* parent)
		{
			this->parent = parent;
			this->scope = new Scope;
			this->scope->next = this->scope->prev = 0;
		}

		~CompilerContext()
		{
			delete this->scope;
		}

		CValue Number(double value)
		{
			return CValue(&DoubleType, llvm::ConstantFP::get(parent->context, llvm::APFloat(value)));
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
				Error("undeclared identifier '" + name + "'", *current_token);
			return value;
		}

		llvm::StoreInst* Store(const std::string& name, CValue val)
		{
			//for each scope
			CValue value = GetVariable(name);

			//iter->second.val->dump();
			//val.val->dump();
			//val.val->getType()->dump();
			val = this->DoCast(value.type, val);

			return parent->builder.CreateStore(val.val, value.val);
		}

		CValue Load(const std::string& name)
		{
			CValue value = GetVariable(name);

			return CValue(value.type, parent->builder.CreateLoad(value.val, name.c_str()));
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
			//todo
			//insert new label after this
			if (loops.empty())
				Error("Cannot continue from outside loop!", *current_token);

			this->parent->builder.CreateBr(loops.top().second);
		}

		void Break()
		{
			//todo
			//insert new label after this
			if (loops.empty())
				Error("Cannot break from outside loop!", *current_token);

			this->parent->builder.CreateBr(loops.top().first);
		}

		llvm::ReturnInst* Return(CValue ret)
		{
			//try and cast if we can
			if (this->function == 0)
				Error("Cannot return from outside function!", *current_token);

			//call destructors
			auto cur = this->scope;
			do
			{
				for (auto ii: cur->named_values)
				{
					if (ii.second.type->type == Types::Class)
					{
						//look for destructor
						auto destructor = ii.second.type->data->functions.find("destroy");
						if (destructor != ii.second.type->data->functions.end())
						{
							//call it
							this->Call("destroy", { CValue(this->parent->LookupType(ii.second.type->ToString() + "*"), ii.second.val) }, ii.second.type);
						}
					}
				}
				cur = cur->prev;
			} while (cur);

				ret = this->DoCast(this->function->return_type, ret);
			return parent->builder.CreateRet(ret.val);
		}

		Token* current_token;
		inline void CurrentToken(Token* token)
		{
			current_token = token;
		}

		void PushScope()
		{
			//todo
			auto temp = this->scope;
			this->scope = new Scope;
			this->scope->prev = temp;
			this->scope->next = 0;
			temp->next = this->scope;
		}

		void PopScope()
		{
			//call destructors
			if (this->scope->prev != 0 && this->scope->prev->prev != 0)
			{
				for (auto ii : this->scope->named_values)
				{
					if (ii.second.type->type == Types::Class)
					{
						//look for destructor
						auto destructor = ii.second.type->data->functions.find("destroy");
						if (destructor != ii.second.type->data->functions.end())
						{
							//call it
							this->Call("destroy", { CValue(this->parent->LookupType(ii.second.type->ToString() + "*"), ii.second.val) }, ii.second.type);
						}
					}
				}
			}

			auto temp = this->scope;
			this->scope = this->scope->prev;
			delete temp;
			//call destructors
		}

		CValue DoCast(Type* t, CValue value)
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
			}
			if (value.type->type == Types::Pointer)
			{
				//pointer to bool
				if (t->type == Types::Bool)
					return CValue(t, parent->builder.CreateIsNotNull(value.val));
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
						/*std::vector<llvm::Value*> arr = { parent->builder.getInt32(0) };// , parent->builder.getInt32(0)

						value.val->dump();
						value.val = parent->builder.CreateGEP(value.val, arr, "array2ptr");
						value.val->dump();
						return CValue(t, value.val);*/
					}
				}
			}

			Error("Cannot cast '" + value.type->ToString() + "' to '" + t->ToString() + "'!", *current_token);
		}

		CompilerContext* AddFunction(const std::string& fname, Type* ret, const std::vector<std::pair<Type*, std::string>>& args, bool member);

		CValue Call(const std::string& name, const std::vector<CValue>& args, Type* Struct = 0)
		{
			llvm::Function* f = 0;
			Function* fun = 0;

			if (Struct == 0)
			{
				//global function?
				auto iter = this->parent->functions.find(name);
				if (iter == this->parent->functions.end())
					Error("Function '" + name + "' is not defined", *this->current_token);

				fun = iter->second;
			}
			else
			{
				//im a struct yo
				auto iter = Struct->data->functions.find(name);
				if (iter == Struct->data->functions.end())
					Error("Function '" + name + "' is not defined on object '" + Struct->ToString() + "'", *this->current_token);

				fun = iter->second;
			}

			fun->Load(this->parent);

			f = this->parent->module->getFunction(fun->name);

			if (args.size() != f->arg_size())
			{
				//todo: add better checks later
				Error("Mismatched function parameters in call", *this->current_token);
			}

			std::vector<llvm::Value*> argsv;
			int i = 0;
			for (auto ii : args)
			{
				//try and cast to the correct type if we can
				argsv.push_back(this->DoCast(fun->argst[i++].first, ii).val);
			}
			return CValue(fun->return_type, this->parent->builder.CreateCall(f, argsv));
		}
	};
};
#endif