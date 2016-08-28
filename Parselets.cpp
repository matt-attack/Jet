#include "Parselets.h"
#include "Expressions.h"
#include "Parser.h"
#include "UniquePtr.h"

#include <string>

using namespace Jet;

#ifdef _DEBUG
#ifndef DBG_NEW
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#define new DBG_NEW
#endif
#endif  // _DEBUG


//todo: fix this being a parser hack that drops potential tokens
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

	while (parser->MatchAndConsume(TokenType::Asterisk))//parse pointers
		out += '*';

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
				parser->Error("Cannot size array with a zero or negative size", tok);

			out += "[" + std::to_string((int)s->GetIntValue()) + "]";
		}
		else
		{
			parser->Error("Cannot size array with a non constant size", tok);
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
	if (parser->Match(TokenType::TemplateBegin))//TokenType::Not))
	{
		//check if the token after the next this is a , or a > if it is, then im a template
		//Token ahead = parser->LookAhead(1);
		//if (ahead.type != TokenType::Not)//ahead.type != TokenType::Comma && ahead.type != TokenType::GreaterThan)
		//return new NameExpression(token);

		parser->Consume();
		//parser->Consume(TokenType::LessThan);

		//token.text += "<";
		//parse it as if it is a template
		std::vector<Token>* templates = new std::vector < Token >();
		bool first = true;
		do
		{
			//if (first)
			//	first = false;
			//else
			//	token.text += ",";
			Token tname = ::ParseType(parser);
			//token.text += tname.text;
			templates->push_back(tname);
		} while (parser->MatchAndConsume(TokenType::Comma));
		parser->Consume(TokenType::GreaterThan);

		//token.text += ">";

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
	return new TypeofExpression(token, left, x, right);
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
	auto end = parser->Consume(TokenType::RightParen);
	return new GroupExpression(token, exp.Release(), end);
}

Expression* WhileParselet::parse(Parser* parser, Token token)
{
	auto ob = parser->Consume(TokenType::LeftParen);

	UniquePtr<Expression*> condition = parser->ParseExpression();

	auto cb = parser->Consume(TokenType::RightParen);

	auto block = new ScopeExpression(parser->ParseBlock());
	return new WhileExpression(token, ob, condition.Release(), cb, block);
}

Expression* CaseParselet::parse(Parser* parser, Token token)
{
	Token number = parser->Consume(TokenType::Number);

	Token colon = parser->Consume(TokenType::Colon);

	return new CaseExpression(token, number, colon);
}

Expression* DefaultParselet::parse(Parser* parser, Token token)
{
	auto colon = parser->Consume(TokenType::Colon);

	return new DefaultExpression(token, colon);
}

Expression* ForParselet::parse(Parser* parser, Token token)
{
	auto ob = parser->Consume(TokenType::LeftParen);

	UniquePtr<Expression*> initial = 0;
	if (!parser->Match(TokenType::Semicolon))
		initial = parser->ParseStatement(false);
	auto s1 = parser->Consume();
	UniquePtr<Expression*> condition = 0;
	if (!parser->Match(TokenType::Semicolon))
		condition = parser->ParseStatement(false);
	auto s2 = parser->Consume();
	UniquePtr<Expression*> increment = 0;
	if (!parser->Match(TokenType::RightParen))
		increment = parser->ParseExpression();

	auto cb = parser->Consume(TokenType::RightParen);

	auto block = new ScopeExpression(parser->ParseBlock());
	return new ForExpression(token, ob, initial.Release(), s1, condition.Release(), s2, increment.Release(), cb, block);
}

Expression* SwitchParselet::parse(Parser* parser, Token token)
{
	auto ob = parser->Consume(TokenType::LeftParen);
	UniquePtr<Expression*> var = parser->ParseExpression();
	auto cb = parser->Consume(TokenType::RightParen);

	BlockExpression* block = parser->ParseBlock(false);

	return new SwitchExpression(token, ob, var.Release(), cb, block);
}

Expression* UnionParselet::parse(Parser* parser, Token token)
{
	Token name = parser->Consume(TokenType::Name);

	Token equals = parser->Consume(TokenType::Assign);

	std::vector<std::pair<Token, Token>> elements;
	while (parser->LookAhead().type != TokenType::Semicolon)
	{
		//parse in each block
		Token name = parser->Consume(TokenType::Name);

		Token bor = parser->LookAhead();
		if (bor.type == TokenType::BOr)
		{
			parser->Consume();
			elements.push_back({ name, bor });
			continue;
		}

		elements.push_back({ name, Token() });

		break;
	}

	return new UnionExpression(token, name, equals, std::move(elements));
}

Expression* MatchParselet::parse(Parser* parser, Token token)
{
	Token open = parser->Consume(TokenType::LeftParen);
	UniquePtr<Expression*> var = parser->ParseExpression();
	Token close = parser->Consume(TokenType::RightParen);

	Token ob = parser->Consume(TokenType::LeftBrace);

	std::vector<MatchCase> cases;
	while (!parser->Match(TokenType::RightBrace))
	{
		//parse in each block
		if (parser->LookAhead().type == TokenType::Default)
		{
			Token def = parser->Consume();

			auto tok = parser->Consume(TokenType::Assign);//fix this not being one token
			parser->Consume(TokenType::GreaterThan);
			tok.text += ">";

			BlockExpression* block = parser->ParseBlock(true);

			cases.push_back({ def, def, tok, block });

			break;
		}

		Token type = parser->Consume(TokenType::Name);
		Token name = parser->Consume(TokenType::Name);
		Token tok = parser->Consume(TokenType::Assign);

		parser->Consume(TokenType::GreaterThan);
		tok.text += ">";
		BlockExpression* block = parser->ParseBlock(true);

		cases.push_back({ type, name, tok, block });
	}

	Token cb = parser->Consume();

	return new MatchExpression(token, open, close, var.Release(), ob, std::move(cases), cb);
}

Expression* IfParselet::parse(Parser* parser, Token token)
{
	std::vector<Branch*> branches;
	//take parens
	auto ob = parser->Consume(TokenType::LeftParen);
	UniquePtr<Expression*> ifcondition = parser->ParseExpression();
	auto cb = parser->Consume(TokenType::RightParen);

	BlockExpression* ifblock = parser->ParseBlock(true);
	branches.push_back(new Branch(token, ob, cb, new ScopeExpression(ifblock), ifcondition.Release()));

	Branch* Else = 0;
	while (true)
	{
		//look for elses
		Token t = parser->LookAhead();
		if (t.type == TokenType::ElseIf)
		{
			parser->Consume();

			//keep going
			auto ob = parser->Consume(TokenType::LeftParen);
			UniquePtr<Expression*> condition = parser->ParseExpression();
			auto cb = parser->Consume(TokenType::RightParen);

			BlockExpression* block = parser->ParseBlock(true);

			branches.push_back(new Branch(t, ob, cb, new ScopeExpression(block), condition.Release()));
		}
		else if (t.type == TokenType::Else)
		{
			parser->Consume();

			//its an else
			BlockExpression* block = parser->ParseBlock(true);

			Else = new Branch(t, Token(), Token(), new ScopeExpression(block), 0);
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

	Token tcb, tob;
	//parse templates
	std::vector<TraitTemplate>* templated = 0;
	if (parser->Match(TokenType::LessThan))
	{
		tob = parser->Consume();
		templated = new std::vector < TraitTemplate > ;
		//parse types and stuff
		do
		{
			Token ttname = parser->Consume(TokenType::Name);
			Token tname = parser->LookAhead();
			if (tname.type == TokenType::Comma)
			{
				parser->Consume();
				templated->push_back({ ttname, tname });
			}
			else
			{
				templated->push_back({ ttname, Token() });
				break;
			}

			//templated->push_back({ ttname, tname });
		} while (true);//parser->MatchAndConsume(TokenType::Comma));
		tcb = parser->Consume(TokenType::GreaterThan);
	}

	auto ob = parser->Consume(TokenType::LeftBrace);

	std::vector<TraitFunction> functions;
	while (parser->Match(TokenType::RightBrace) == false)
	{
		TraitFunction func;
		Token tfunc = parser->Consume(TokenType::Function);
		func.ret_type = ::ParseType(parser);
		func.name = parser->Consume(TokenType::Name);
		func.func_token = tfunc;
		func.open_brace = parser->Consume(TokenType::LeftParen);

		bool first = true;
		while (parser->Match(TokenType::RightParen) == false)
		{
			if (first)
			{
				first = false;
				//parse args
				auto type = ::ParseType(parser);
				func.args.push_back({ type, parser->Consume(TokenType::Name), Token() });
			}
			else
			{
				auto comma = parser->Consume(TokenType::Comma);
				//parse args
				auto type = ::ParseType(parser);
				func.args.push_back({ type, parser->Consume(TokenType::Name), comma });
			}
		}

		func.close_brace = parser->Consume();
		func.semicolon = parser->Consume(TokenType::Semicolon);
		functions.push_back(func);
	}
	auto cb = parser->Consume();

	return new TraitExpression(token, name, tob, tcb, ob, std::move(functions), templated, cb);
}

Expression* StructParselet::parse(Parser* parser, Token token)
{
	Token name = parser->Consume(TokenType::Name);
	Token ob, cb;
	//parse templates
	std::vector<StructTemplate>* templated = 0;
	if (parser->Match(TokenType::LessThan))
	{
		ob = parser->Consume();
		templated = new std::vector < StructTemplate > ;
		//parse types and stuff
		do
		{
			Token ttname = ParseTrait(parser);
			Token tname = parser->Consume(TokenType::Name);
			Token comma = parser->LookAhead();
			if (comma.type == TokenType::Comma)
			{
				templated->push_back({ ttname, tname, comma });
				parser->Consume();
			}
			else
			{
				templated->push_back({ ttname, tname, Token() });
				break;
			}
		} while (true);
		cb = parser->Consume(TokenType::GreaterThan);
	}

	//parse base type
	Token base_name, colon;
	if (parser->Match(TokenType::Colon))
	{
		colon = parser->Consume();
		base_name = ::ParseType(parser, false);
	}

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
		else if (parser->Match(TokenType::Struct))
		{
			//parse the function
			auto* expr = parser->ParseStatement(true);

			if (auto fun = dynamic_cast<StructExpression*>(expr))
			{
				for (auto ii : fun->members)
				{
					if (ii.type == StructMember::FunctionMember)
						parser->Error("Member struct definitions cannot have member functions!", token);
				}
				
				StructMember member;
				member.type = StructMember::DefinitionMember;
				member.definition = fun;
				members.push_back(member);
				continue;
			}

			parser->Error("Member struct definition must have body!", token);
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

		Token semicolon = parser->Consume(TokenType::Semicolon);

		StructMember member;
		member.type = StructMember::VariableMember;
		member.variable = { type, name, semicolon };
		members.push_back(member);
	}

	return new StructExpression(token, name, start, end, ob, std::move(members), /*elements, functions,*/ templated, cb, colon, base_name);
}


bool IsValidFunctionNameToken(TokenType op)
{
	//todo: speed this up somehow
	if (op == TokenType::Name)
		return true;
	else if (op == TokenType::Plus)
		return true;
	else if (op == TokenType::Minus)
		return true;
	else if (op == TokenType::Slash)
		return true;
	else if (op == TokenType::Asterisk)
		return true;
	else if (op == TokenType::LeftShift)
		return true;
	else if (op == TokenType::RightShift)
		return true;
	else if (op == TokenType::BOr)
		return true;
	else if (op == TokenType::BAnd)
		return true;
	else if (op == TokenType::Xor)
		return true;
	else if (op == TokenType::Modulo)
		return true;
	else if (op == TokenType::LeftBracket)
		return true;
	else if (op == TokenType::LessThan)
		return true;
	else if (op == TokenType::GreaterThan)
		return true;
	else if (op == TokenType::GreaterThanEqual)
		return true;
	else if (op == TokenType::LessThanEqual)
		return true;

	return false;
}
//maybe add a way to make non - nullable non - freeable pointers
//add superconst to represent const int* const x;
//still gotta solve operator problem with passing operands as pointers
//ok, [] operator can be implemented by returning a pointer to the value
Expression* FunctionParselet::parse(Parser* parser, Token token)
{
	//read in type
	Token ret_type = ::ParseType(parser);
	
	//todo fix this not allowing binary not operator
	Token name;
	if (parser->Match(TokenType::BNot))
	{
		Token t = parser->Consume();
		Token rname = parser->Consume(TokenType::Name);
		if (rname.trivia_length > 0)
			parser->Error("Cannot have whitespace between ~ and the classname in destructors", rname);
		name = rname;
		name.text = "~" + rname.text;
		name.text_ptr -= 1;
		name.trivia_length = t.trivia_length;
	}
	else
		name = parser->Consume(TokenType::Name);
	//bool destructor = parser->MatchAndConsume(TokenType::BNot);//todo: fix this parser hack

	auto arguments = new std::vector < FunctionArg > ;

	Token stru, colons;
	if (parser->Match(TokenType::Scope))
	{
		//its a extension method definition
		colons = parser->Consume();
		stru = name;
		name = parser->Consume(TokenType::Name);//parse the real function name
	}

	Token oper;
	if (name.text == "operator")
	{
		//parse in the operator
		oper = name;
		name = parser->Consume();
		if (name.type == TokenType::LeftBracket)
		{
			auto name2 = parser->Consume(TokenType::RightBracket);
			name.text += ']';
		}
	}

	//check that the name is ok
	if (stru.text.length() && stru.type != TokenType::Name)
		parser->Error("Invalid struct name", stru);
	else if (!IsValidFunctionNameToken(name.type))
		parser->Error("Not a valid operator overload", name);


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
	Token ob = parser->Consume(TokenType::LeftParen);

	if (!parser->Match(TokenType::RightParen))
	{
		do
		{
			Token type = ::ParseType(parser);

			Token name = parser->Consume();
			Token comma = parser->LookAhead();
			if (name.type == TokenType::Name)
			{
				if (comma.type == TokenType::Comma)
					arguments->push_back({ type, name, comma });
				else
					arguments->push_back({ type, name, Token() });
			}
			else
			{
				std::string str = "Token not as expected! Expected name or got: " + name.text;
				parser->Error(str, name);
			}
		} while (parser->MatchAndConsume(TokenType::Comma));
	}
	Token cb = parser->Consume(TokenType::RightParen);

	//check that there are a proper number of arguments if this is a operator overload
	if (name.type != TokenType::Name)
	{
		//todo: fix unary minus
		if ((name.type == TokenType::Decrement
			|| name.type == TokenType::Increment
			|| name.type == TokenType::BNot
			/*|| name.type == TokenType::Minus*/) && arguments->size() != 0)
		{
			//todo put more unary ops here
			parser->Error("Wrong number of arguments for operator overload '" + name.text + "' expected 0 got " + std::to_string(arguments->size()), (*arguments)[arguments->size() - 1].name);
		}
		else if (name.text == "[]" && arguments->size() != 1 && arguments->size() != 0)//.type == TokenType::DoubleBracket)//[] operator
		{
			parser->Error("Wrong number of arguments for operator overload '" + name.text + "' expected 1 or 0 got " + std::to_string(arguments->size()), (*arguments)[arguments->size() - 1].name);
		}
		else if (arguments->size() != 1)
		{
			//binary ops
			parser->Error("Wrong number of arguments for operator overload '" + name.text + "' expected 1 got " + std::to_string(arguments->size()), (*arguments)[arguments->size() - 1].name);
		}
	}

	auto block = new ScopeExpression(parser->ParseBlock());
	return new FunctionExpression(token, name, ret_type, token.type == TokenType::Generator, arguments, block, /*varargs,*/ stru, colons, templated, 0, ob, cb, oper);
}

Expression* ExternParselet::parse(Parser* parser, Token token)
{
	auto fun = parser->Consume(TokenType::Function);

	Token ret_type = ::ParseType(parser);

	Token name = parser->Consume();
	if (name.type == TokenType::Free)
	{
		name.type == TokenType::Name;
	}
	else if (name.type != TokenType::Name)
	{
		std::string str = "Token Not As Expected! Expected: " + TokenToString[TokenType::Name] + " Got: " + name.text;

		//it was probably forgotten, insert dummy
		//fabricate a fake token
		parser->Error(str, name);//need to make this throw again

		//if (name.type != TokenType::Semicolon)
		throw 7;
	}
	auto arguments = new std::vector < ExternArg > ;

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
	auto ob = parser->Consume(TokenType::LeftParen);

	Token cb;
	if (!parser->Match(TokenType::RightParen))
	{
		do
		{
			Token type = ::ParseType(parser);
			Token name = parser->Consume(TokenType::Name);
			if (name.type == TokenType::Name)
			{
				auto comma = parser->LookAhead();
				if (comma.type == TokenType::Comma)
					arguments->push_back({ type, name, comma });
				else
					arguments->push_back({ type, name, Token() });
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

		cb = parser->Consume(TokenType::RightParen);
	}
	else
	{
		cb = parser->Consume();
	}

	return new ExternExpression(token, fun, name, ret_type, ob, arguments, cb, stru);
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

		auto cb = parser->Consume(TokenType::RightBracket);

		//parse the next thing

		auto thing = parser->ParseStatement(false);

		//ok, add the attribute to the list ok, we need an attribute expression

		auto attr = new AttributeExpression(token, attribute, cb, thing);

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
			Token name = parser->Consume(TokenType::Number);
			captures->push_back(name);
			break;
		} while (parser->MatchAndConsume(TokenType::Comma));
	}
	if (captures->size() == 0)
	{
		delete captures;
		captures = 0;
	}

	parser->Consume(TokenType::RightBracket);

	Token ob = parser->Consume(TokenType::LeftParen);

	auto arguments = new std::vector < FunctionArg > ;
	/*if (parser->LookAhead().type != TokenType::RightParen)
	{
		do
		{
			Token type;
			if (parser->LookAhead(1).type != TokenType::Comma && parser->LookAhead(1).type != TokenType::RightParen)
				type = ::ParseType(parser);

			Token name = parser->Consume();
			Token comma = parser->LookAhead();
			if (name.type == TokenType::Name)
			{
				if (comma.type == TokenType::Comma)
					arguments->push_back({ type, name, comma });
				else
				{
					arguments->push_back({ type, name, Token() });
					break;
				}
			}
			else
			{
				std::string str = "Token not as expected! Expected name got: " + name.text;
				parser->Error(str, token);
			}
		} while (true);// parser->MatchAndConsume(TokenType::Comma));
	}*/

	if (!parser->Match(TokenType::RightParen))
	{
		do
		{
			Token type = ::ParseType(parser);

			Token name = parser->Consume();
			Token comma = parser->LookAhead();
			if (name.type == TokenType::Name)
			{
				if (comma.type == TokenType::Comma)
					arguments->push_back({ type, name, comma });
				else
					arguments->push_back({ type, name, Token() });
			}
			else
			{
				std::string str = "Token not as expected! Expected name or got: " + name.text;
				parser->Error(str, name);
			}
		} while (parser->MatchAndConsume(TokenType::Comma));
	}

	auto cb = parser->Consume(TokenType::RightParen);
	Token ret_type;
	if (parser->MatchAndConsume(TokenType::Pointy))
		ret_type = ::ParseType(parser);

	auto block = new ScopeExpression(parser->ParseBlock());
	return new FunctionExpression(token, Token(), ret_type, false, arguments, block, Token(), Token(), 0, captures, ob, cb, Token());
}

Expression* CallParselet::parse(Parser* parser, Expression* left, Token token)
{
	UniquePtr<std::vector<std::pair<Expression*, Token>>*> arguments = new std::vector < std::pair<Expression*, Token> > ;
	if (!parser->Match(TokenType::RightParen))
	{
		do
		{
			auto expr = parser->ParseExpression(Precedence::ASSIGNMENT);
			auto comma = parser->LookAhead();
			if (comma.type == TokenType::Comma)
				arguments->push_back({ expr, comma });
			else
			{
				arguments->push_back({ expr, Token() });
			}
		} while (parser->MatchAndConsume(TokenType::Comma));

		//cb = parser->Consume(TokenType::RightParen);
	}
	Token cb = parser->Consume(TokenType::RightParen);
	return new CallExpression(token, cb, left, arguments.Release());
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
			UniquePtr<Expression*> size = parser->ParseExpression(Precedence::ASSIGNMENT);

			parser->Consume(TokenType::RightBracket);

			if (auto s = dynamic_cast<NumberExpression*>((Expression*)size))
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
		names->push_back({ type, name });
	} while (parser->MatchAndConsume(TokenType::Comma));

	if (parser->Match(TokenType::Semicolon))
		return new LocalExpression(token, Token(), names.Release(), 0);

	Token equals = parser->Consume(TokenType::Assign);//its possible this wont be here and it may just be a mentioning, but no assignment

	//handle multiple comma expressions
	UniquePtr<std::vector<std::pair<Token, Expression*>>*> rights = new std::vector < std::pair<Token, Expression*> > ;
	do
	{
		Expression* right = parser->ParseExpression(Precedence::ASSIGNMENT - 1/*assignment prcedence -1 */);

		auto tok = parser->LookAhead();
		if (tok.type == TokenType::Comma)
			rights->push_back({ tok, right });
		else
			rights->push_back({ Token(), right });
	} while (parser->MatchAndConsume(TokenType::Comma));

	return new LocalExpression(token, equals, names.Release(), rights.Release());
}

Expression* ConstParselet::parse(Parser* parser, Token token)
{
	parser->Error("Not Implemented", token);

	auto names = new std::vector < std::pair<Token, Token> > ;
	/*do
	{
	Token name = parser->Consume(TokenType::Name);
	//names->push_back(name);
	} while (parser->MatchAndConsume(TokenType::Comma));

	parser->Consume(TokenType::Assign);//its possible this wont be here and it may just be a mentioning, but no assignment

	//do somethign with multiple comma expressions
	std::vector<Expression*>* rights = new std::vector < Expression* > ;
	do
	{
	Expression* right = parser->ParseExpression(Precedence::ASSIGNMENT - 1);

	rights->push_back(right);
	} while (parser->MatchAndConsume(TokenType::Comma));

	parser->Consume(TokenType::Semicolon);*/
	//do stuff with this and store and what not
	//need to add this variable to this's block expression

	return 0;// new LocalExpression(token, names, rights);
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
	auto cb = parser->Consume(TokenType::RightBracket);

	return new IndexExpression(left, index.Release(), token, cb);
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

	return new IndexExpression(left, name->token, token);
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

	return new IndexExpression(left, name->token, token);
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
	UniquePtr<std::vector<std::pair<Token, Token>>*> names = new std::vector < std::pair<Token, Token> > ;

	while (true)
	{
		Token name = parser->Consume(TokenType::Name);

		if (parser->Match(TokenType::Scope) == false)
		{
			names->push_back({ name, Token() });
			break;
		}

		Token colons = parser->Consume();
		names->push_back({ name, colons });
	}

	auto block = parser->ParseBlock(false);

	return new NamespaceExpression(token, names.Release(), block);
}

Expression* NewParselet::parse(Parser* parser, Token token)
{
	auto type = ::ParseType(parser, false);

	//try and parse size expression
	if (parser->LookAhead().type == TokenType::LeftBracket)//auto tok = parser->MatchAndConsume(TokenType::LeftBracket))
	{
		auto ob = parser->Consume();

		UniquePtr<Expression*> size = parser->ParseExpression(Precedence::ASSIGNMENT);

		auto cb = parser->Consume(TokenType::RightBracket);

		auto x = new NewExpression(token, type, size.Release());
		x->open_bracket = ob;
		x->close_bracket = cb;
		return x;
	}
	return new NewExpression(token, type, 0);
}

Expression* FreeParselet::parse(Parser* parser, Token token)
{
	//auto type = ::ParseType(parser, false);

	//try and parse size expression
	if (parser->LookAhead().type == TokenType::LeftBracket)//auto tok = parser->MatchAndConsume(TokenType::LeftBracket))
	{
		auto ob = parser->Consume();
		auto cb = parser->Consume(TokenType::RightBracket);

		UniquePtr<Expression*> pointer = parser->ParseExpression(Precedence::ASSIGNMENT);

		auto x = new FreeExpression(token, pointer.Release());
		x->open_bracket = ob;
		x->close_bracket = cb;
		return x;
	}

	return new FreeExpression(token, parser->ParseExpression(Precedence::ASSIGNMENT));
}

struct EnumValue
{
	Token name, equals, value;
	Token comma;
};

Expression* EnumParselet::parse(Parser* parser, Token token)
{
	auto name = parser->Consume(TokenType::Name);

	auto start = parser->Consume(TokenType::LeftBrace);

	std::vector<EnumValue> values;
	do
	{
		auto ename = parser->Consume(TokenType::Name);

		Token equals, value;
		if (parser->Match(TokenType::Assign))
		{
			equals = parser->Consume();
			value = parser->Consume(TokenType::Number);
		}

		if (parser->LookAhead(0).type != TokenType::Comma)
		{
			values.push_back({ ename, equals, value, Token() });
			break;
		}
		else
			values.push_back({ ename, equals, value, parser->Consume() });
	} while (true);

	auto end = parser->Consume(TokenType::RightBrace);

	return new EnumExpression(token, name, start, end, std::move(values));// 0;
}