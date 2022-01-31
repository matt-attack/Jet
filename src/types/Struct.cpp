#include "Types.h"
#include "../Compiler.h"
#include "../CompilerContext.h"
#include <expressions/Expressions.h>
#include <expressions/DeclarationExpressions.h>
#include "../Lexer.h"
#include "Function.h"

using namespace Jet;

bool is_constructor(const std::string& name)
{
	std::string strname;
	for (unsigned int i = 2; i < name.length(); i++)
	{
		if (name[i] == '_')
			break;

		strname.push_back(name[i]);
	}

	auto sub = name.substr(strname.length() + 3, name.length());
	if (name.length() == strname.length() * 2 + 3 && sub == strname)
		return true;
	return false;
}

void Struct::Load(Compilation* compiler)
{
	if (this->loaded)
		return;

	//add on items from parent
	bool needs_vtable = false;
	int vtable_size = 0;
	if (this->parent_struct)
	{
		this->parent_struct->Load(compiler);
		//check parent for vtable
		//	is just moving all the members ok, or do I need to compress vtables
		if (this->parent_struct->type != Types::Struct)
			compiler->Error("Struct's parent must be another Struct!", *compiler->current_function->current_token);

		//add its members to me
		auto oldmem = std::move(this->struct_members);
		this->struct_members.clear();
		//dont add constructors
		for (auto ii : this->parent_struct->data->struct_members)
			this->struct_members.push_back(ii);
		for (auto ii : oldmem)
			this->struct_members.push_back(ii);

		//add the functions too :D
		auto oldfuncs = std::move(this->functions);
		//ok, now remove extra destructors
		this->functions.clear();
		for (auto ii : this->parent_struct->data->functions)
		{
			//setup vtable indices
			this->functions.insert(ii);

			//exclude any constructors, or missing functions
			bool ic = is_constructor(ii.second->name_);
			if (ic || ii.first == this->parent_struct->data->name || ii.second == 0)
				continue;

			if (ii.second->virtual_offset_ + 1 > vtable_size)
				vtable_size = ii.second->virtual_offset_ + 1;
		}
		for (auto ii : oldfuncs)
		{
			bool duplicate = false; int dup_loc = 0;
			std::string dupname = ii.first.find('~') != -1 ? "~" + this->parent_struct->data->name : ii.first;
			for (auto mem : this->parent_struct->data->functions)
			{
				if (mem.first == dupname)
				{
					duplicate = true;
					dup_loc = mem.second->virtual_offset_;

					//lets just erase it
					this->functions.erase(ii.first);
					break;
				}
			}

			//todo have it warn/info if no virtual is suggested
			this->functions.insert(ii);

			//exclude any constructors, or missing functions
			if (ii.first == this->name || ii.second == 0)
				continue;

			assert(ii.second->virtual_offset_ == -1);

			if (duplicate)
				ii.second->virtual_offset_ = dup_loc;
			else
				ii.second->virtual_offset_ = vtable_size++;
		}
	}
	else
	{
		//populate vtable with my own functions
		for (auto ii : this->functions)
		{
			//exclude any constructors, or missing functions
			if (ii.first == this->name || ii.second == 0)
				continue;

			if (ii.second->is_virtual_)
				ii.second->virtual_offset_ = vtable_size++;
		}
	}

	if (vtable_size > 0)
		needs_vtable = true;//lets do it always just because (need to only do it when im inherited from)

	// Add the vtable if we are the first in the tree and need it
	if (needs_vtable && this->parent_struct == 0)
	{
		this->struct_members.push_back({ "__vtable", "char*", compiler->CharPointerType->GetPointerType() });
	}

	//recursively load
	llvm::StructType* struct_type = 0;
	std::vector<llvm::Type*> elementss;
	int i = 0;
	for (auto ii : this->struct_members)
	{
		auto type = ii.type;

		if (ii.type->type == Types::Struct && ii.type->data == this || ii.type->GetBaseType()->data == this)
		{
			if (ii.type->type == Types::Pointer)
			{
				//we are good, do a cheat load
				if (struct_type == 0)
					struct_type = llvm::StructType::create(compiler->context, this->name);
				elementss.push_back(struct_type->getPointerTo());
			}
			else
			{
				if (this->expression)
				{
					compiler->Error("Circular dependency in class \"" + this->name +  "\"", this->expression->members[i].variable.type);
				}
				else
					compiler->Error("Circular dependency in class \"" + this->name + "\"", *compiler->current_function->current_token);
			}
		}
		else
		{
			ii.type->Load(compiler);

			elementss.push_back(type->GetLLVMType());
		}
		i++;
	}
	if (elementss.size() == 0)
	{
		//add dummy element
		elementss.push_back(compiler->IntType->GetLLVMType());
	}

	if (struct_type == 0)
		this->type = llvm::StructType::create(elementss, this->name, true);
	else
	{
		struct_type->setBody(elementss, true);
		this->type = struct_type;
	}
	this->loaded = true;


	//populate vtable
	//vtables need to be indentical in layout!!!
	if (needs_vtable)
	{
		std::vector<llvm::Constant*> ptrs;
		ptrs.resize(vtable_size);

		//find the vtable location
		unsigned int vtable_loc = 0;
		for (; vtable_loc < this->struct_members.size(); vtable_loc++)
		{
			if (this->struct_members[vtable_loc].name == "__vtable")
				break;
		}

		//load the functions first, then add all the virtual ones
		//first add all virtuals from the parent, then all of mine
		for (auto ii : this->functions)
		{
			if (ii.second == 0 || ii.second->virtual_offset_ == -1)
				continue;

			//ii.second->is_virtual = true;
			ii.second->virtual_table_location_ = vtable_loc;

			ii.second->Load(compiler);
			auto ptr = ii.second->f_;
			auto charptr = llvm::ConstantExpr::getBitCast(ptr, compiler->CharPointerType->GetLLVMType());

			ptrs[ii.second->virtual_offset_] = charptr;
		}

		auto arr = llvm::ConstantArray::get(llvm::ArrayType::get(compiler->CharPointerType->GetLLVMType(), vtable_size), ptrs);

		auto oldns = compiler->ns;

		compiler->ns = this;
		//then need to have function calls to virtual functions to go the lookup table
		compiler->AddGlobal("__" + this->name + "_vtable", compiler->CharPointerType, vtable_size, arr, true);

		compiler->ns = oldns;
	}
}


bool Struct::IsParent(Type* ty)
{
	//ok this needs to check down the line
	auto current = this->parent_struct;
	while (current)
	{
		if (current == ty)
			return true;
		current = current->data->parent_struct;
	}

	return false;
}
