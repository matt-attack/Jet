#pragma once

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

#include "Token.h"
//#include "JetInstructions.h"
//#include "JetExceptions.h"

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

	class BlockExpression;

	enum class Types
	{
		Void,
		Double,
		Float,
		Int,
		Char,
		Short,
		Bool,

		Class,//value type
		Function,

		Pointer,

		Invalid,//for unloaded types
	};


	class Compiler;
	struct Struct;
	struct Type
	{
		Types type : 8;
		bool loaded : 8;
		union
		{
			Struct* data;//for classes
			Type* base;//for pointers
		};

		Type() { data = 0; type = Types::Void; loaded = false; }
		Type(Types type, Struct* data = 0) : type(type), data(data), loaded(false) {}
		Type(Types type, Type* base) : type(type), base(base), loaded(false) {}

		void Load(Compiler* compiler);

		std::string ToString();
	};

	struct Struct
	{
		std::string name;
		std::vector<std::pair<std::string, Type*>> members;
		llvm::Type* type;

		bool loaded;

		Struct()
		{
			type = 0;
			loaded = false;
		}

		void Load(Compiler* compiler);
	};

	extern Type VoidType;
	extern Type BoolType;
	extern Type DoubleType;
	extern Type IntType;

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

	struct Function
	{
		std::string name;

		std::vector<llvm::Type*> args;
		std::vector<std::pair<Type*, std::string>> argst;

		llvm::Function* f;//not always used

		Type* return_type;

		bool loaded;

		Function()
		{
			loaded = false;
		}

		void Load(Compiler* compiler);
	};

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
				OurFPM.run(*ii.second->f);
			}
		}

		void Dump()
		{
			if (module)
				module->dump();
		}

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
					auto t = types[name.substr(0, name.length() - 1)];

					type = new Type;
					type->base = t;
					type->type = Types::Pointer;

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

	llvm::Type* GetType(Type* t);

	//compiles functions
	class CompilerContext
	{

	public:
		Compiler* parent;

		llvm::Function* f;

		std::map<std::string, CValue> named_values;

		Function* function;

		CompilerContext(Compiler* parent)
		{
			this->parent = parent;
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

		llvm::StoreInst* Store(const std::string& name, CValue val)
		{
			auto iter = named_values.find(name);
			if (iter == named_values.end())
				Error("undeclared identifier '" + name + "'", *current_token);

			return parent->builder.CreateStore(val.val, iter->second.val);
		}

		CValue Load(const std::string& name)
		{
			auto iter = named_values.find(name);
			if (iter == named_values.end())
				Error("undeclared identifier '"+name+"'", *current_token);
			return CValue(iter->second.type, parent->builder.CreateLoad(iter->second.val, name.c_str()));
		}


		CValue UnaryOperation(TokenType operation, CValue value)
		{
			llvm::Value* res = 0;

			if (value.type->type == Types::Float || value.type->type == Types::Double)
			{
				switch (operation)
				{
				case TokenType::Increment:
					//res = parent->builder.CreateFAdd(left.val, right.val);
					break;
				case TokenType::Decrement:
					//res = parent->builder.CreateFSub(left.val, right.val);
					break;
				case TokenType::Minus:
					res = parent->builder.CreateFNeg(value.val);// parent->builder.CreateFMul(left.val, right.val);
					break;
				default:
					throw 7;
					break;
				}

				return CValue(value.type, res);
			}
			else if (value.type->type == Types::Int || value.type->type == Types::Short || value.type->type == Types::Char)
			{
				//integer probably
				switch (operation)
				{
				case TokenType::Increment:
					//res = parent->builder.CreateFAdd(left.val, right.val);
					break;
				case TokenType::Decrement:
					//res = parent->builder.CreateFSub(left.val, right.val);
					break;
				case TokenType::Minus:
					res = parent->builder.CreateNeg(value.val);// parent->builder.CreateFMul(left.val, right.val);
					break;
				case TokenType::BNot:
					res = parent->builder.CreateNot(value.val);
					break;
				default:
					throw 7;
					break;
				}

				return CValue(value.type, res);
			}
			//throw 7;
			Error("Invalid Unary Operation!", *current_token);
		}

		CValue BinaryOperation(Jet::TokenType op, CValue left, CValue right)
		{
			llvm::Value* res = 0;

			//should this be a floating point calc?
			if (left.type->type != right.type->type)
			{
				//conversion time!!

				throw 7;
			}

			if (left.type->type == Types::Float || left.type->type == Types::Double)
			{
				switch (op)
				{
				case TokenType::AddAssign:
				case TokenType::Plus:
					res = parent->builder.CreateFAdd(left.val, right.val);
					break;
				case TokenType::SubtractAssign:
				case TokenType::Minus:
					res = parent->builder.CreateFSub(left.val, right.val);
					break;
				case TokenType::MultiplyAssign:
				case TokenType::Asterisk:
					res = parent->builder.CreateFMul(left.val, right.val);
					break;
				case TokenType::DivideAssign:
				case TokenType::Slash:
					res = parent->builder.CreateFDiv(left.val, right.val);
					break;
				case TokenType::LessThan:
					//use U or O?
					res = parent->builder.CreateFCmpULT(left.val, right.val);
					return CValue(&BoolType, res);
					break;
				case TokenType::LessThanEqual:
					res = parent->builder.CreateFCmpULE(left.val, right.val);
					return CValue(&BoolType, res);
					break;
				case TokenType::GreaterThan:
					res = parent->builder.CreateFCmpUGT(left.val, right.val);
					return CValue(&BoolType, res);
					break;
				case TokenType::GreaterThanEqual:
					res = parent->builder.CreateFCmpUGE(left.val, right.val);
					return CValue(&BoolType, res);
					break;
				case TokenType::Equals:
					res = parent->builder.CreateFCmpUEQ(left.val, right.val);
					return CValue(&BoolType, res);
					break;
				case TokenType::NotEqual:
					res = parent->builder.CreateFCmpUNE(left.val, right.val);
					return CValue(&BoolType, res);
					break;
				default:
					Error("Invalid Binary Operation!", *current_token);
					//throw 7;
					break;
				}

				return CValue(left.type, res);
			}
			else if (left.type->type == Types::Int || left.type->type == Types::Short || left.type->type == Types::Char)
			{
				//integer probably
				switch (op)
				{
				case TokenType::AddAssign:
				case TokenType::Plus:
					res = parent->builder.CreateAdd(left.val, right.val);
					break;
				case TokenType::SubtractAssign:
				case TokenType::Minus:
					res = parent->builder.CreateSub(left.val, right.val);
					break;
				case TokenType::MultiplyAssign:
				case TokenType::Asterisk:
					res = parent->builder.CreateMul(left.val, right.val);
					break;
				case TokenType::DivideAssign:
				case TokenType::Slash:
					if (true)//signed
						res = parent->builder.CreateSDiv(left.val, right.val);
					else//unsigned
						res = parent->builder.CreateUDiv(left.val, right.val);
					break;
				case TokenType::Modulo:
					if (true)//signed
						res = parent->builder.CreateSRem(left.val, right.val);
					else//unsigned
						res = parent->builder.CreateURem(left.val, right.val);
					break;
					//todo add unsigned
				case TokenType::LessThan:
					//use U or S?
					res = parent->builder.CreateICmpSLT(left.val, right.val);
					return CValue(&BoolType, res);
					break;
				case TokenType::LessThanEqual:
					res = parent->builder.CreateICmpSLE(left.val, right.val);
					return CValue(&BoolType, res);
					break;
				case TokenType::GreaterThan:
					res = parent->builder.CreateICmpSGT(left.val, right.val);
					return CValue(&BoolType, res);
					break;
				case TokenType::GreaterThanEqual:
					res = parent->builder.CreateICmpSGE(left.val, right.val);
					return CValue(&BoolType, res);
					break;
				case TokenType::Equals:
					res = parent->builder.CreateICmpEQ(left.val, right.val);
					return CValue(&BoolType, res);
					break;
				case TokenType::NotEqual:
					res = parent->builder.CreateICmpNE(left.val, right.val);
					return CValue(&BoolType, res);
					break;
				case TokenType::BAnd:
				case TokenType::AndAssign:
					res = parent->builder.CreateAnd(left.val, right.val);
					break;
				case TokenType::BOr:
				case TokenType::OrAssign:
					res = parent->builder.CreateOr(left.val, right.val);
					break;
				case TokenType::Xor:
				case TokenType::XorAssign:
					res = parent->builder.CreateXor(left.val, right.val);
					break;
				case TokenType::LeftShift:
					//todo
				case TokenType::RightShift:
					//todo
				default:
					Error("Invalid Binary Operation!", *current_token);
					//throw 7;
					break;
				}

				return CValue(left.type, res);
			}
			//printf("Invalid Binary Operation!\n");

			Error("Invalid Binary Operation!", *current_token);
			//throw 7;
			//return res;
		}

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
			{
				Error("Cannot return from outside function!", *current_token);
			}

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
		}

		void PopScope()
		{
			//todo
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

			Error("Invalid Cast!", *current_token);

			//printf("Invalid Cast!\n");
			//throw 7; //unhandled cast
		}

		CompilerContext* AddFunction(const std::string& fname, Type* ret, const std::vector<std::pair<Type*, std::string>>& args);
	};

	//per function context, or w/e
	/*class CompilerContext
	{
	friend class FunctionExpression;
	friend class CallExpression;
	std::map<std::string, CompilerContext*> functions;

	struct LoopInfo
	{
	std::string Break;
	std::string Continue;
	int locals;//local index at which loop starts
	};
	std::vector<LoopInfo> loops;

	struct LocalVariable
	{
	int local;
	std::string name;
	};

	struct Scope
	{
	Scope* previous;
	Scope* next;
	int level;
	std::vector<LocalVariable> localvars;
	};
	Scope* scope;//linked list starting at current scope

	unsigned int localindex;//next open local index

	bool vararg; bool isgenerator;
	unsigned int closures;//number of closures we have
	unsigned int arguments;//number of arguments we have

	CompilerContext* parent;//parent scoping function

	std::vector<IntermediateInstruction> out;//list of instructions generated

	public:

	CompilerContext(void);
	~CompilerContext(void);

	void PrintAssembly();

	//used when generating functions
	CompilerContext* AddFunction(std::string name, unsigned int args, bool vararg = false);
	void FinalizeFunction(CompilerContext* c);

	std::vector<IntermediateInstruction> Compile(BlockExpression* expr, const char* filename);

	private:
	void Compile()
	{
	//append functions to end here
	for (auto fun: this->functions)
	{
	fun.second->Compile();

	//need to set var with the function name and location
	this->FunctionLabel(fun.first, fun.second->arguments, fun.second->localindex, fun.second->closures, fun.second->vararg, fun.second->isgenerator);
	for (auto ins: fun.second->out)
	this->out.push_back(ins);

	//add code of functions recursively
	}
	}

	void FunctionLabel(std::string name, int args, int locals, int upvals, bool vararg = false, bool isgenerator = false)
	{
	IntermediateInstruction ins = IntermediateInstruction(InstructionType::Function, name, args);
	ins.a = args;
	ins.b = locals;
	ins.c = upvals;
	ins.d = vararg + isgenerator*2;
	out.push_back(ins);
	}

	public:

	void PushScope()
	{
	Scope* s = new Scope;
	this->scope->next = s;
	s->level = this->scope->level + 1;
	s->previous = this->scope;
	s->next = 0;
	this->scope = s;
	}

	void PopScope()
	{
	if (this->scope && this->scope->previous)
	this->scope = this->scope->previous;

	if (this->scope)
	{
	delete this->scope->next;
	this->scope->next = 0;
	}
	}

	void PushLoop(const std::string Break, const std::string Continue)
	{
	LoopInfo i;
	i.Break = Break;
	i.Continue = Continue;
	i.locals = this->localindex;
	loops.push_back(i);
	}

	void PopLoop()
	{
	//close ALL variables
	//do a close if necessary
	//whoops, need to have this in the blocks, not here

	//close all closures in the loop
	//if (found)
	out.push_back(IntermediateInstruction(InstructionType::Close, loops.back().locals));

	loops.pop_back();
	}

	void Break()
	{
	if (this->loops.size() == 0)
	throw CompilerException(this->filename, this->lastline, "Cannot use break outside of a loop!");
	this->Jump(loops.back().Break.c_str());
	}

	void Continue()
	{
	if (this->loops.size() == 0)
	throw CompilerException(this->filename, this->lastline, "Cannot use continue outside of a loop!");
	this->Jump(loops.back().Continue.c_str());
	}

	void ForEach(const std::string& dest, const std::string& start, std::string& end)
	{

	}

	bool RegisterLocal(const std::string name);//returns success

	void BinaryOperation(TokenType operation);
	void UnaryOperation(TokenType operation);

	//stack operations
	void Pop()
	{
	out.push_back(IntermediateInstruction(InstructionType::Pop));
	}

	void Duplicate()
	{
	out.push_back(IntermediateInstruction(InstructionType::Dup));
	}

	//load operations
	void Null()
	{
	out.push_back(IntermediateInstruction(InstructionType::LdNull));
	}
	void Number(double value)
	{
	out.push_back(IntermediateInstruction(InstructionType::LdNum, 0, value));
	}

	void String(std::string string)
	{
	out.push_back(IntermediateInstruction(InstructionType::LdStr, string.c_str()));
	}

	//jumps
	void JumpFalse(const char* pos)
	{
	out.push_back(IntermediateInstruction(InstructionType::JumpFalse, pos));
	}

	void JumpTrue(const char* pos)
	{
	out.push_back(IntermediateInstruction(InstructionType::JumpTrue, pos));
	}

	void JumpFalsePeek(const char* pos)
	{
	out.push_back(IntermediateInstruction(InstructionType::JumpFalsePeek, pos));
	}

	void JumpTruePeek(const char* pos)
	{
	out.push_back(IntermediateInstruction(InstructionType::JumpTruePeek, pos));
	}

	void Jump(const char* pos)
	{
	out.push_back(IntermediateInstruction(InstructionType::Jump, pos));
	}

	void Label(const std::string& name)
	{
	out.push_back(IntermediateInstruction(InstructionType::Label, name));
	}

	struct Capture
	{
	int localindex;
	int level;
	int captureindex;
	bool uploaded;
	Capture() {}

	Capture(int l, int li, int ci) : localindex(li), level(l), captureindex(ci) {uploaded = false;}
	};
	std::map<std::string, Capture> captures;

	void Store(const std::string variable);

	void StoreLocal(const std::string variable)
	{
	//look up if I am local or global
	this->Store(variable);
	}

	//this loads locals and globals atm
	void Load(const std::string variable);

	bool IsLocal(const std::string variable)
	{
	Scope* ptr = this->scope;
	while (ptr)
	{
	//look for var in locals
	for (unsigned int i = 0; i < ptr->localvars.size(); i++)
	{
	if (ptr->localvars[i].name == variable)
	{
	//printf("We found loading of a local var: %s at level %d, index %d\n", variable.c_str(), ptr->level, ptr->localvars[i].first);
	//this->output += ".local " + variable + " " + ::std::to_string(i) + ";\n";
	return true;//we found it
	}
	}
	if (ptr)
	ptr = ptr->previous;
	}

	auto cur = this->parent;
	while(cur)
	{
	ptr = cur->scope;
	while (ptr)
	{
	//look for var in locals
	for (unsigned int i = 0; i < ptr->localvars.size(); i++)
	{
	if (ptr->localvars[i].name == variable)
	{
	//printf("We found loading of a captured var: %s at level %d, index %d\n", variable.c_str(), level, ptr->localvars[i].first);
	//this->output += ".local " + variable + " " + ::std::to_string(i) + ";\n";
	return true;
	}
	}
	if (ptr)
	ptr = ptr->previous;
	}
	cur = cur->parent;
	}
	return false;
	}

	void LoadFunction(const std::string name)
	{
	out.push_back(IntermediateInstruction(InstructionType::LoadFunction, name));
	}

	void Call(const std::string function, unsigned int args)
	{
	out.push_back(IntermediateInstruction(InstructionType::Call, function, args));
	}

	void ECall(unsigned int args)
	{
	out.push_back(IntermediateInstruction(InstructionType::ECall, args));
	}

	void LoadIndex(const char* index = 0)
	{
	out.push_back(IntermediateInstruction(InstructionType::LoadAt, index));
	}

	void StoreIndex(const char* index = 0)
	{
	out.push_back(IntermediateInstruction(InstructionType::StoreAt, index));
	}

	void NewArray(unsigned int number)
	{
	out.push_back(IntermediateInstruction(InstructionType::NewArray, number));
	}

	void NewObject(unsigned int number)
	{
	out.push_back(IntermediateInstruction(InstructionType::NewObject, number));
	}

	void Return()
	{
	//if (this->closures > 0)//close all open closures
	out.push_back(IntermediateInstruction(InstructionType::Close));
	out.push_back(IntermediateInstruction(InstructionType::Return));
	}

	void Yield()
	{
	out.push_back(IntermediateInstruction(InstructionType::Yield));
	this->isgenerator = true;
	}

	void Resume()
	{
	out.push_back(IntermediateInstruction(InstructionType::Resume));
	}

	//debug info
	std::string filename;
	void SetFilename(const std::string filename)
	{
	this->filename = filename;
	}

	unsigned int lastline;
	void Line(unsigned int line)
	{
	//need to avoid duplicates, because thats silly
	if (lastline != line)
	{
	lastline = line;
	out.push_back(IntermediateInstruction(InstructionType::DebugLine, filename, line));
	}
	}

	private:
	int uuid;

	public:
	std::string GetUUID()//use for autogenerated labels
	{
	return std::to_string(uuid++);
	}
	};*/

};