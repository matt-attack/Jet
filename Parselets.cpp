#include "Parselets.h"
#include "Expressions.h"
#include "Parser.h"
#include "UniquePtr.h"

#include <string>

using namespace Jet;


Token ParseType(Parser* parser, bool parse_arrays = true)
{
	Token name = parser->Consume(TokenType::Name);
	std::string out = name.text;

	//parse namespaces
	while (parser->MatchAndConsume(TokenType::Scope))
	{
		out += "::";
		out += parser->Consume(TokenType::Name).text;
	}

	if (parser->MatchAndConsume(TokenType::LessThan))//parse templates
	{
		out += "<";
		//recursively parse the rest
		bool first = true;
		do
		{
			if (first == false)
				out += ",";
			first = false;
			out += ParseType(parser).text;
		} while (parser->MatchAndConsume(TokenType::Comma));

		parser->ConsumeTemplateGT();
		//parser->Consume(TokenType::GreaterThan);
		out += ">";
	}
	else if (parser->MatchAndConsume(TokenType::LeftParen))
	{
		out += "(";

		if (!parser->MatchAndConsume(TokenType::RightParen))
		{
			//recursively parse the rest
			bool first = true;
			do
			{
				if (first == false)
					out += ",";
				first = false;
				out += ParseType(parser).text;
			} while (parser->MatchAndConsume(TokenType::Comma));

			parser->Consume(TokenType::RightParen);
		}
		out += ")";
	}

	while (parser->MatchAndConsume(TokenType::Asterisk))//parse pointers
		out += '*';

	//parse arrays
	if (parse_arrays && parser->MatchAndConsume(TokenType::LeftBracket))
	{
		//its an array type
		//read the number
		auto size = parser->ParseExpression(Precedence::ASSIGNMENT);

		auto tok = parser->Consume(TokenType::RightBracket);

		if (auto s = dynamic_cast<NumberExpression*>(size))
		{
			if (s->GetIntValue() <= 0)
			{
				parser->Error("Cannot size array with a zero or negative size", tok);
				//throw 7;
			}
			out += "[" + std::to_string((int)s->GetIntValue()) + "]";
		}
		else
		{
			parser->Error("Cannot size array with a non constant size", tok);
			//throw 7;
		}
	}

	Token ret;
	ret.column = name.column;
	ret.line = name.line;
	ret.trivia_length = name.trivia_length;
	ret.text_ptr = name.text_ptr;
	ret.text = out;
	return ret;
}

Token ParseTrait(Parser* parser)
{
	Token name = parser->Consume(TokenType::Name);
	std::string out = name.text;

	//parse templates
	if (parser->MatchAndConsume(TokenType::LessThan))
	{
		out += "<";
		//recursively parse the rest
		bool first = true;
		do
		{
			if (first == false)
				out += ",";
			first = false;
			out += ParseType(parser).text;
		} while (parser->MatchAndConsume(TokenType::Comma));

		parser->Consume(TokenType::GreaterThan);
		out += ">";
	}

	Token ret;
	ret.column = name.column;
	ret.line = name.line;
	ret.trivia_length = name.trivia_length;
	ret.text_ptr = name.text_ptr;
	ret.text = out;
	return ret;
}

Expression* NameParselet::parse(Parser* parser, Token token)
{
	if (parser->Match(TokenType::Not))
	{
		//check if the token after the next this is a , or a > if it is, then im a template
		//Token ahead = parser->LookAhead(1);
		//if (ahead.type != TokenType::Not)//ahead.type != TokenType::Comma && ahead.type != TokenType::GreaterThan)
			//return new NameExpression(token);

		parser->Consume();
		parser->Consume(TokenType::LessThan);

		token.text += "<";
		//parse it as if it is a template
		std::vector<Token>* templates = new std::vector < Token >();
		bool first = true;
		do
		{
			if (first)
				first = false;
			else
				token.text += ",";
			Token tname = ::ParseType(parser);
			token.text += tname.text;
			templates->push_back(tname);
		} while (parser->MatchAndConsume(TokenType::Comma));
		parser->Consume(TokenType::GreaterThan);

		token.text += ">";

		//actually, just treat me as a nameexpression, that should work
		//idk, but what to do with the template stuff
		//ok need to use these templates
		return new NameExpression(token, templates);
	}
	return new NameExpression(token);
}

Expression* AssignParselet::parse(Parser* parser, Expression* left, Token token)
{
	Expression* right = parser->ParseExpression(Precedence::ASSIGNMENT - 1/*assignment prcedence -1 */);

	if (dynamic_cast<IStorableExpression*>(left) == 0)
	{
		delete right;
		parser->Error("AssignParselet: Left hand side must be a storable location!", token);
	}
	return new AssignExpression(token, left, right);
}

Expression* ScopeParselet::parse(Parser* parser, Expression* left, Token token)
{
	Expression* right = parser->ParseExpression(Precedence::ASSIGNMENT - 1/*assignment prcedence -1 */);

	return new ScopedExpression(token, left, right);
}


Expression* OperatorAssignParselet::parse(Parser* parser, Expression* left, Token token)
{
	if (dynamic_cast<IStorableExpression*>(left) == 0)
		parser->Error("OperatorAssignParselet: Left hand side must be a storable location!", token);

	Expression* right = parser->ParseExpression(Precedence::ASSIGNMENT - 1);

	return new OperatorAssignExpression(token, left, right);
}

Expression* PrefixOperatorParselet::parse(Parser* parser, Token token)
{
	Expression* right = parser->ParseExpression(precedence);
	if (right == 0)
		parser->Error("PrefixOperatorParselet: Right hand side missing!", token);

	return new PrefixExpression(token, right);
}

Expression* SizeofParselet::parse(Parser* parser, Token token)
{
	Token left = parser->Consume(TokenType::LeftParen);
	Token type = ::ParseType(parser);
	Token right = parser->Consume(TokenType::RightParen);
	return new SizeofExpression(token, left, type, right);
}

Expression* TypeofParselet::parse(Parser* parser, Token token)
{
	Token left = parser->Consume(TokenType::LeftParen);
	auto x = parser->ParseExpression(0);
	Token right = parser->Consume(TokenType::RightParen);
	return 0;
}

Expression* CastParselet::parse(Parser* parser, Token token)
{
	Token type = ::ParseType(parser);
	Token end = parser->Consume(TokenType::GreaterThan);

	auto right = parser->ParseExpression(Precedence::PREFIX);
	if (right == 0)
		parser->Error("CastParselet: Right hand side missing!", token);

	return new CastExpression(type, token, right, end);
}

Expression* BinaryOperatorParselet::parse(Parser* parser, Expression* left, Token token)
{
	Expression* right = parser->ParseExpression(precedence - (isRight ? 1 : 0));
	if (right == 0)
		parser->Error("BinaryOperatorParselet: Right hand side missing!", token);

	return new OperatorExpression(left, token, right);
}

Expression* GroupParselet::parse(Parser* parser, Token token)
{
	UniquePtr<Expression*> exp = parser->ParseExpression();
	parser->Consume(TokenType::RightParen);
	return exp.Release();
}

Expression* WhileParselet::parse(Parser* parser, Token token)
{
	parser->Consume(TokenType::LeftParen);

	UniquePtr<Expression*> condition = parser->ParseExpression();

	parser->Consume(TokenType::RightParen);

	auto block = new ScopeExpression(parser->ParseBlock());
	return new WhileExpression(token, condition.Release(), block);
}

Expression* CaseParselet::parse(Parser* parser, Token token)
{
	int number = std::stol(parser->Consume(TokenType::Number).text);

	parser->Consume(TokenType::Colon);

	return new CaseExpression(token, number);
}

Expression* DefaultParselet::parse(Parser* parser, Token token)
{
	parser->Consume(TokenType::Colon);

	return new DefaultExpression(token);
}

Expression* ForParselet::parse(Parser* parser, Token token)
{
	parser->Consume(TokenType::LeftParen);
	/*if (parser->LookAhead().type == TokenType::Local)
	{
	if (parser->LookAhead(1).type == TokenType::Name)
	{
	Token n = parser->LookAhead(2);
	if (n.type == TokenType::Name && n.text == "in")
	{
	//ok its a foreach loop
	parser->Consume();
	auto name = parser->Consume();
	parser->Consume();
	UniquePtr<Expression*> container = parser->ParseExpression();
	parser->Consume(TokenType::RightParen);
	throw 7;
	auto block = new ScopeExpression(parser->ParseBlock());
	//return new ForEachExpression(name, container.Release(), block);
	}
	}
	}*/


	/*UniquePtr<Expression*> initial = 0;
	//if (!parser->MatchAndConsume(TokenType::Semicolon))
	initial = parser->ParseStatement(true);
	UniquePtr<Expression*> condition = 0;
	//if (!parser->MatchAndConsume(TokenType::Semicolon))
	condition = parser->ParseStatement(true);
	UniquePtr<Expression*> increment = 0;
	//if (!parser->MatchAndConsume(TokenType::Semicolon))
	increment = parser->ParseExpression();*/


	UniquePtr<Expression*> initial = 0;
	if (!parser->MatchAndConsume(TokenType::Semicolon))
		initial = parser->ParseStatement(true);
	UniquePtr<Expression*> condition = 0;
	if (!parser->MatchAndConsume(TokenType::Semicolon))
		condition = parser->ParseStatement(true);
	UniquePtr<Expression*> increment = 0;
	if (!parser->Match(TokenType::RightParen))
		increment = parser->ParseExpression();

	parser->Consume(TokenType::RightParen);

	auto block = new ScopeExpression(parser->ParseBlock());
	return new ForExpression(token, initial.Release(), condition.Release(), increment.Release(), block);
}

Expression* SwitchParselet::parse(Parser* parser, Token token)
{
	parser->Consume(TokenType::LeftParen);
	UniquePtr<Expression*> var = parser->ParseExpression();
	parser->Consume(TokenType::RightParen);

	BlockExpression* block = parser->ParseBlock(false);

	return new SwitchExpression(token, var.Release(), block);
}

Expression* UnionParselet::parse(Parser* parser, Token token)
{
	Token name = parser->Consume(TokenType::Name);

	parser->Consume(TokenType::Assign);

	std::vector<Token> elements;
	while (parser->LookAhead().type != TokenType::Semicolon)
	{
		//parse in each block
		Token name = parser->Consume(TokenType::Name);

		elements.push_back(name);

		if (parser->MatchAndConsume(TokenType::BOr))
			continue;

		break;
	}

	return new UnionExpression(name, std::move(elements));
	//return new SwitchExpression(token, var.Release(), block);
}

Expression* MatchParselet::parse(Parser* parser, Token token)
{
	parser->Consume(TokenType::LeftParen);
	UniquePtr<Expression*> var = parser->ParseExpression();
	parser->Consume(TokenType::RightParen);

	parser->Consume(TokenType::LeftBrace);

	std::vector<MatchCase> cases;
	while (!parser->MatchAndConsume(TokenType::RightBrace))
	{
		//parse in each block
		if (parser->LookAhead().type == TokenType::Default)
		{
			Token def = parser->Consume();

			auto tok = parser->Consume(TokenType::Assign);
			parser->Consume(TokenType::GreaterThan);

			BlockExpression* block = parser->ParseBlock(true);

			cases.push_back({ def, def, block });

			parser->Consume(TokenType::RightBrace);
			break;
		}

		Token type = parser->Consume(TokenType::Name);

		Token name = parser->Consume(TokenType::Name);

		auto tok = parser->Consume(TokenType::Assign);
		parser->Consume(TokenType::GreaterThan);

		BlockExpression* block = parser->ParseBlock(true);

		cases.push_back({ type, name, block });
	}

	return new MatchExpression(token, var.Release(), std::move(cases));
	//return new SwitchExpression(token, var.Release(), block);
}

Expression* IfParselet::parse(Parser* parser, Token token)
{
	std::vector<Branch*> branches;
	//take parens
	parser->Consume(TokenType::LeftParen);
	UniquePtr<Expression*> ifcondition = parser->ParseExpression();
	parser->Consume(TokenType::RightParen);

	BlockExpression* ifblock = parser->ParseBlock(true);

	branches.push_back(new Branch(token, ifblock, ifcondition.Release()));

	Branch* Else = 0;
	while (true)
	{
		//look for elses
		Token t = parser->LookAhead();
		if (t.type == TokenType::ElseIf)
		{
			parser->Consume();

			//keep going
			parser->Consume(TokenType::LeftParen);
			UniquePtr<Expression*> condition = parser->ParseExpression();
			parser->Consume(TokenType::RightParen);

			BlockExpression* block = parser->ParseBlock(true);

			branches.push_back(new Branch(t, block, condition.Release()));
		}
		else if (t.type == TokenType::Else)
		{
			parser->Consume();

			//its an else
			BlockExpression* block = parser->ParseBlock(true);

			Else = new Branch(t, block, 0);
			break;
		}
		else
			break;//nothing else
	}

	return new IfExpression(token, std::move(branches), Else);
}

Expression* TraitParselet::parse(Parser* parser, Token token)
{
	Token name = parser->Consume(TokenType::Name);

	//parse templates
	std::vector<std::pair<Token, Token>>* templated = 0;
	if (parser->MatchAndConsume(TokenType::LessThan))
	{
		templated = new std::vector < std::pair<Token, Token> > ;
		//parse types and stuff
		do
		{
			Token ttname = parser->Consume(TokenType::Name);
			Token tname = parser->LookAhead();
			if (tname.type == TokenType::Name)
				parser->Consume(TokenType::Name);
			else
			{
				tname = ttname;
				ttname = Token();
			}

			templated->push_back({ ttname, tname });
		} while (parser->MatchAndConsume(TokenType::Comma));
		parser->Consume(TokenType::GreaterThan);
	}

	parser->Consume(TokenType::LeftBrace);

	std::vector<TraitFunction> functions;
	while (parser->MatchAndConsume(TokenType::RightBrace) == false)
	{
		TraitFunction func;
		Token tfunc = parser->Consume(TokenType::Function);
		func.ret_type = ::ParseType(parser);
		func.name = parser->Consume(TokenType::Name);

		parser->Consume(TokenType::LeftParen);

		bool first = true;
		while (parser->MatchAndConsume(TokenType::RightParen) == false)
		{
			if (first)
				first = false;
			else
				parser->Consume(TokenType::Comma);

			//parse args
			func.args.push_back(::ParseType(parser));
			Token arg = parser->Consume(TokenType::Name);
		}

		functions.push_back(func);

		parser->Consume(TokenType::Semicolon);
	}

	return new TraitExpression(token, name, std::move(functions), templated);
}

Expression* StructParselet::parse(Parser* parser, Token token)
{
	Token name = parser->Consume(TokenType::Name);

	//parse templates
	std::vector<std::pair<Token, Token>>* templated = 0;
	if (parser->MatchAndConsume(TokenType::LessThan))
	{
		templated = new std::vector < std::pair<Token, Token> > ;
		//parse types and stuff
		do
		{
			Token ttname = ParseTrait(parser);
			Token tname = parser->Consume(TokenType::Name);

			templated->push_back({ ttname, tname });
		} while (parser->MatchAndConsume(TokenType::Comma));
		parser->Consume(TokenType::GreaterThan);
	}

	//parse base type
	Token base_name;
	if (parser->MatchAndConsume(TokenType::Colon))
		base_name = parser->Consume(TokenType::Name);

	auto start = parser->Consume(TokenType::LeftBrace);

	std::vector<StructMember> members;
	Token end;
	while (true)
	{
		if (parser->Match(TokenType::RightBrace))
		{
			end = parser->Consume(TokenType::RightBrace);
			break;
		}

		//first read type
		if (parser->Match(TokenType::Function))
		{
			//parse the function
			auto* expr = parser->ParseStatement(true);

			if (auto fun = dynamic_cast<FunctionExpression*>(expr))
			{
				StructMember member;
				member.type = StructMember::FunctionMember;
				member.function = fun;
				members.push_back(member);
				continue;
			}

			parser->Error("Member function definition must have body!", token);
		}
		else if (parser->Match(TokenType::Generator))
		{
			parser->Error("Struct member generators not yet implemented!", token);

			//parse the function
			auto* expr = parser->ParseStatement(true);

			if (auto fun = dynamic_cast<FunctionExpression*>(expr))
			{
				StructMember member;
				member.type = StructMember::FunctionMember;
				member.function = fun;
				members.push_back(member);
				continue;
			}

			parser->Error("Member function definition must have body!", token);
		}

		Token type = ::ParseType(parser);

		Token name = parser->Consume(TokenType::Name);//then read name

		if (parser->MatchAndConsume(TokenType::LeftBracket))
		{
			//its an array type
			//read the number
			auto size = parser->ParseExpression(Precedence::ASSIGNMENT);

			parser->Consume(TokenType::RightBracket);

			if (auto s = dynamic_cast<NumberExpression*>(size))
			{
				if (s->GetIntValue() <= 0)
					parser->Error("Cannot size array with a zero or negative size", token);

				type.text += "[" + std::to_string((int)s->GetIntValue()) + "]";
			}
			else
			{
				parser->Error("Cannot size array with a non constant size", token);
			}
		}

		parser->Consume(TokenType::Semicolon);

		StructMember member;
		member.type = StructMember::VariableMember;
		member.variable = { type, name };
		members.push_back(member);
	}

	return new StructExpression(token, name, start, end, std::move(members), /*elements, functions,*/ templated, base_name);
}

Expression* FunctionParselet::parse(Parser* parser, Token token)
{
	//read in type
	Token ret_type = ::ParseType(parser);

	bool destructor = parser->MatchAndConsume(TokenType::BNot);
	Token name = parser->Consume(TokenType::Name);
	if (destructor)
		name.text = "~" + name.text;
	auto arguments = new std::vector < std::pair<std::string, std::string> > ;

	Token stru;
	if (parser->MatchAndConsume(TokenType::Scope))
	{
		//its a struct definition
		stru = name;
		name = parser->Consume(TokenType::Name);//parse the real function name
	}

	//look for templates
	std::vector<std::pair<Token, Token>>* templated = 0;
	if (parser->MatchAndConsume(TokenType::LessThan))
	{
		templated = new std::vector < std::pair<Token, Token> > ;
		//parse types and stuff
		do
		{
			Token ttname = ParseTrait(parser);
			Token tname = parser->Consume(TokenType::Name);

			templated->push_back({ ttname, tname });
		} while (parser->MatchAndConsume(TokenType::Comma));
		parser->Consume(TokenType::GreaterThan);
	}

	NameExpression* varargs = 0;
	parser->Consume(TokenType::LeftParen);

	if (!parser->MatchAndConsume(TokenType::RightParen))
	{
		do
		{
			Token type = ::ParseType(parser);

			Token name = parser->Consume();
			if (name.type == TokenType::Name)
			{
				arguments->push_back({ type.text, name.text });
			}
			else
			{
				std::string str = "Token not as expected! Expected name or got: " + name.text;
				parser->Error(str, name);
			}
		} while (parser->MatchAndConsume(TokenType::Comma));

		parser->Consume(TokenType::RightParen);
	}

	auto block = new ScopeExpression(parser->ParseBlock());
	return new FunctionExpression(token, name, ret_type, token.type == TokenType::Generator, arguments, block, /*varargs,*/ stru, templated);
}

Expression* ExternParselet::parse(Parser* parser, Token token)
{
	parser->Consume(TokenType::Function);

	Token ret_type = ::ParseType(parser);

	//add calling convention setting here
	//if (parser->LookAhead()

	Token name = parser->Consume(TokenType::Name);
	auto arguments = new std::vector < std::pair<std::string, std::string> > ;

	std::string stru;
	if (parser->MatchAndConsume(TokenType::Scope))
	{
		//its a struct definition
		stru = name.text;
		bool destructor = parser->MatchAndConsume(TokenType::BNot);
		name = parser->Consume(TokenType::Name);//parse the real function name
		if (destructor)
			name.text = "~" + name.text;
	}

	NameExpression* varargs = 0;
	parser->Consume(TokenType::LeftParen);

	if (!parser->MatchAndConsume(TokenType::RightParen))
	{
		do
		{
			Token type = ::ParseType(parser);
			Token name = parser->Consume(TokenType::Name);
			if (name.type == TokenType::Name)
			{
				arguments->push_back({ type.text, name.text });
			}
			/*else if (name.type == TokenType::Ellipses)
			{
			varargs = new NameExpression(parser->Consume(TokenType::Name).getText());

			break;//this is end of parsing arguments
			}*/
			else
			{
				std::string str = "Token not as expected! Expected name or got: " + name.text;
				parser->Error(str, name);
			}
		} while (parser->MatchAndConsume(TokenType::Comma));

		parser->Consume(TokenType::RightParen);
	}

	return new ExternExpression(token, name, ret_type, arguments, stru);
}


Expression* LambdaAndAttributeParselet::parse(Parser* parser, Token token)
{
	//if im not in a function parser, see where I am
	//check if im an attribute
	if (parser->LookAhead().type == TokenType::Name && (parser->LookAhead(1).type == TokenType::LeftParen || parser->LookAhead(1).type == TokenType::RightBracket))
	{
		//its an attribute
		Token attribute = parser->Consume();

		if (parser->MatchAndConsume(TokenType::LeftParen))
		{
			//handle arguments for the attribute
		}

		parser->Consume(TokenType::RightBracket);

		//parse the next thing

		auto thing = parser->ParseStatement(false);

		//ok, add the attribute to the list ok, we need an attribute expression

		auto attr = new AttributeExpression(token, attribute, thing);

		if (dynamic_cast<ExternExpression*>(thing))
			return attr;
		else if (dynamic_cast<FunctionExpression*>(thing))
			return attr;

		parser->Error("Attributes can only be applied to extern and function expressions.", attribute);
		return attr;
	}

	//parse captures
	auto captures = new std::vector < Token > ;
	if (parser->LookAhead().type != TokenType::RightBracket)
	{
		do
		{
			Token name = parser->Consume(TokenType::Name);
			captures->push_back(name);
		} while (parser->MatchAndConsume(TokenType::Comma));
	}
	if (captures->size() == 0)
	{
		delete captures;
		captures = 0;
	}

	parser->Consume(TokenType::RightBracket);

	parser->Consume(TokenType::LeftParen);

	auto arguments = new std::vector < std::pair<std::string, std::string> > ;
	if (parser->LookAhead().type != TokenType::RightParen)
	{
		do
		{
			Token type;
			if (parser->LookAhead(1).type != TokenType::Comma && parser->LookAhead(1).type != TokenType::RightParen)
				type = ::ParseType(parser);

			Token name = parser->Consume();
			if (name.type == TokenType::Name)
			{
				arguments->push_back({ type.text, name.text });
			}
			else
			{
				std::string str = "Token not as expected! Expected name got: " + name.text;
				parser->Error(str, token);
			}
		} while (parser->MatchAndConsume(TokenType::Comma));
	}

	parser->Consume(TokenType::RightParen);
	Token ret_type;
	if (parser->MatchAndConsume(TokenType::Pointy))
		ret_type = ::ParseType(parser);

	auto block = new ScopeExpression(parser->ParseBlock());
	return new FunctionExpression(token, Token(), ret_type, false, arguments, block, Token(), 0, captures);
}

Expression* CallParselet::parse(Parser* parser, Expression* left, Token token)
{
	UniquePtr<std::vector<Expression*>*> arguments = new std::vector < Expression* > ;

	if (!parser->MatchAndConsume(TokenType::RightParen))
	{
		do
		{
			arguments->push_back(parser->ParseExpression(Precedence::ASSIGNMENT));
		} while (parser->MatchAndConsume(TokenType::Comma));

		parser->Consume(TokenType::RightParen);
	}
	return new CallExpression(token, left, arguments.Release());
}

Expression* ReturnParselet::parse(Parser* parser, Token token)
{
	Expression* right = 0;
	if (parser->Match(TokenType::Semicolon) == false)
		right = parser->ParseExpression(Precedence::ASSIGNMENT);

	return new ReturnExpression(token, right);
}

Expression* LocalParselet::parse(Parser* parser, Token token)
{
	UniquePtr<std::vector<std::pair<Token, Token>>*> names = new std::vector < std::pair<Token, Token> > ;

	do
	{
		auto next = parser->LookAhead(1);
		Token type;// = next.type == TokenType::Assign ? "" : ParseType(parser).text;
		if (next.type != TokenType::Assign)
			type = ::ParseType(parser);
		Token name = parser->Consume(TokenType::Name);

		if (parser->MatchAndConsume(TokenType::LeftBracket))
		{
			//its an array type
			//read the number
			auto size = parser->ParseExpression(Precedence::ASSIGNMENT);

			parser->Consume(TokenType::RightBracket);

			if (auto s = dynamic_cast<NumberExpression*>(size))
			{
				if (s->GetIntValue() <= 0)
				{
					parser->Error("Cannot size array with a zero or negative size", token);
					//throw 7;
				}
				type.text += "[" + std::to_string((int)s->GetIntValue()) + "]";
			}
			else
			{
				parser->Error("Cannot size array with a non constant size", token);
				//throw 7;
			}
		}
		names->push_back({ type, name });
	} while (parser->MatchAndConsume(TokenType::Comma));

	if (parser->Match(TokenType::Semicolon))
		return new LocalExpression(token, names.Release(), 0);

	parser->Consume(TokenType::Assign);//its possible this wont be here and it may just be a mentioning, but no assignment

	//handle multiple comma expressions
	UniquePtr<std::vector<Expression*>*> rights = new std::vector < Expression* > ;
	do
	{
		Expression* right = parser->ParseExpression(Precedence::ASSIGNMENT - 1/*assignment prcedence -1 */);

		rights->push_back(right);
	} while (parser->MatchAndConsume(TokenType::Comma));

	return new LocalExpression(token, names.Release(), rights.Release());
}

Expression* ConstParselet::parse(Parser* parser, Token token)
{
	parser->Error("Not Implemented", token);

	auto names = new std::vector < std::pair<Token, Token> > ;
	do
	{
		Token name = parser->Consume(TokenType::Name);
		//names->push_back(name);
	} while (parser->MatchAndConsume(TokenType::Comma));

	parser->Consume(TokenType::Assign);//its possible this wont be here and it may just be a mentioning, but no assignment

	//do somethign with multiple comma expressions
	std::vector<Expression*>* rights = new std::vector < Expression* > ;
	do
	{
		Expression* right = parser->ParseExpression(Precedence::ASSIGNMENT - 1/*assignment prcedence -1 */);

		rights->push_back(right);
	} while (parser->MatchAndConsume(TokenType::Comma));

	parser->Consume(TokenType::Semicolon);
	//do stuff with this and store and what not
	//need to add this variable to this's block expression

	return new LocalExpression(token, names, rights);
}

/*Expression* ArrayParselet::parse(Parser* parser, Token token)
{
std::vector<Expression*> inits;// = new std::vector<Expression*>;
while(parser->LookAhead().getType() != TokenType::RightBracket)
{
Expression* e = parser->parseExpression(2);

inits.push_back(e);

if (!parser->MatchAndConsume(TokenType::Comma))//check if more
break;//we are done
}
parser->Consume(TokenType::RightBracket);
return new ArrayExpression(std::move(inits));
}*/

Expression* IndexParselet::parse(Parser* parser, Expression* left, Token token)
{
	UniquePtr<Expression*> index = parser->ParseExpression();
	parser->Consume(TokenType::RightBracket);

	return new IndexExpression(left, index.Release(), token);
}

Expression* MemberParselet::parse(Parser* parser, Expression* left, Token token)
{
	//this is for const members
	Expression* member = parser->ParseExpression(Precedence::CALL);
	UniquePtr<NameExpression*> name = dynamic_cast<NameExpression*>(member);
	if (name == 0)
	{
		delete member; delete left;
		parser->Error("Cannot access member name that is not a string", token);
	}

	auto ret = new IndexExpression(left, name->token, token);

	return ret;
}

Expression* PointerMemberParselet::parse(Parser* parser, Expression* left, Token token)
{
	//this is for const members
	Expression* member = parser->ParseExpression(Precedence::CALL);
	UniquePtr<NameExpression*> name = dynamic_cast<NameExpression*>(member);
	if (name == 0)
	{
		delete member; delete left;
		parser->Error("Cannot access member name that is not a string", token);
	}

	auto ret = new IndexExpression(left, name->token, token);

	return ret;
}

/*Expression* ObjectParselet::parse(Parser* parser, Token token)
{
if (parser->MatchAndConsume(TokenType::RightBrace))
{
//we are done, return null object
return new ObjectExpression();
}

//parse initial values
std::vector<std::pair<std::string, Expression*>>* inits = new std::vector<std::pair<std::string, Expression*>>;
while(parser->LookAhead().type == TokenType::Name || parser->LookAhead().type == TokenType::String || parser->LookAhead().type == TokenType::Number)
{
Token name = parser->Consume();

parser->Consume(TokenType::Assign);

//parse the data;
Expression* e = parser->parseExpression(Precedence::LOGICAL);

inits->push_back(std::pair<std::string, Expression*>(name.text, e));
if (!parser->MatchAndConsume(TokenType::Comma))//is there more to parse?
break;//we are done
}
parser->Consume(TokenType::RightBrace);//end part
return new ObjectExpression(inits);
};*/

Expression* YieldParselet::parse(Parser* parser, Token token)
{
	Expression* right = 0;
	if (parser->Match(TokenType::Semicolon) == false)
		right = parser->ParseExpression(Precedence::ASSIGNMENT);

	return new YieldExpression(token, right);
}

Expression* InlineYieldParselet::parse(Parser* parser, Token token)
{
	Expression* right = 0;
	if (parser->Match(TokenType::Semicolon) == false && parser->LookAhead().type != TokenType::RightParen)
		right = parser->ParseExpression(Precedence::ASSIGNMENT);

	return new YieldExpression(token, right);
}

/*Expression* ResumeParselet::parse(Parser* parser, Token token)
{
Expression* right = parser->parseExpression(Precedence::ASSIGNMENT);

return new ResumeExpression(token, right);
}

Expression* ResumePrefixParselet::parse(Parser* parser, Token token)
{
Expression* right = parser->parseExpression(Precedence::ASSIGNMENT);

return new ResumeExpression(token, right);
}*/

Expression* TypedefParselet::parse(Parser* parser, Token token)
{
	Token new_type = parser->Consume(TokenType::Name);
	Token equals = parser->Consume(TokenType::Assign);
	Token other_type = ::ParseType(parser);
	return new TypedefExpression(token, new_type, equals, other_type);
}

Expression* NamespaceParselet::parse(Parser* parser, Token token)
{
	Token name = parser->Consume(TokenType::Name);

	auto block = parser->ParseBlock(false);

	return new NamespaceExpression(token, name, block);
}

Expression* NewParselet::parse(Parser* parser, Token token)
{
	auto type = ::ParseType(parser, false);

	//try and parse size expression
	Expression* size = 0;
	if (auto tok = parser->MatchAndConsume(TokenType::LeftBracket))
	{
		size = parser->ParseExpression(Precedence::ASSIGNMENT);

		parser->Consume(TokenType::RightBracket);
	}
	return new NewExpression(token, type, size);
}