#include "Parselets.h"
#include "Expressions.h"
#include "Parser.h"
#include "UniquePtr.h"
#include <string>

using namespace Jet;

Expression* NameParselet::parse(Parser* parser, Token token)
{
	return new NameExpression(token);
}

Expression* AssignParselet::parse(Parser* parser, Expression* left, Token token)
{
	Expression* right = parser->parseExpression(Precedence::ASSIGNMENT-1/*assignment prcedence -1 */);

	if (dynamic_cast<IStorableExpression*>(left) == 0)
	{
		delete right;
		ParserError("AssignParselet: Left hand side must be a storable location!", token);
		//throw CompilerException(parser->filename, token.line, "AssignParselet: Left hand side must be a storable location!");
	}
	return new AssignExpression(left, right);
}

Expression* OperatorAssignParselet::parse(Parser* parser, Expression* left, Token token)
{
	if (dynamic_cast<IStorableExpression*>(left) == 0)
		ParserError("OperatorAssignParselet: Left hand side must be a storable location!", token);
		//throw CompilerException(parser->filename, token.line, "OperatorAssignParselet: Left hand side must be a storable location!");

	Expression* right = parser->parseExpression(Precedence::ASSIGNMENT-1);

	return new OperatorAssignExpression(token, left, right);
}

/*Expression* SwapParselet::parse(Parser* parser, Expression* left, Token token)
{
	if (dynamic_cast<IStorableExpression*>(left) == 0)
		throw CompilerException(parser->filename, token.line, "SwapParselet: Left hand side must be a storable location!");

	UniquePtr<Expression*> right = parser->parseExpression(Precedence::ASSIGNMENT-1);

	if (dynamic_cast<IStorableExpression*>((Expression*)right) == 0)
		throw CompilerException(parser->filename, token.line, "SwapParselet: Right hand side must be a storable location!");

	return new SwapExpression(left, right.Release());
}*/

Expression* PrefixOperatorParselet::parse(Parser* parser, Token token)
{
	Expression* right = parser->parseExpression(precedence);
	if (right == 0)
		ParserError("PrefixOperatorParselet: Right hand side missing!", token);
		//throw CompilerException(parser->filename, token.line, "PrefixOperatorParselet: Right hand side missing!");

	return new PrefixExpression(token, right);
}

Expression* BinaryOperatorParselet::parse(Parser* parser, Expression* left, Token token)
{
	Expression* right = parser->parseExpression(precedence - (isRight ? 1 : 0));
	if (right == 0)
		ParserError("BinaryOperatorParselet: Right hand side missing!", token);
		//throw CompilerException(parser->filename, token.line, "BinaryOperatorParselet: Right hand side missing!");

	return new OperatorExpression(left, token, right);
}

Expression* GroupParselet::parse(Parser* parser, Token token)
{
	UniquePtr<Expression*> exp = parser->parseExpression();
	parser->Consume(TokenType::RightParen);
	return exp.Release();
}

Expression* WhileParselet::parse(Parser* parser, Token token)
{
	parser->Consume(TokenType::LeftParen);

	UniquePtr<Expression*> condition = parser->parseExpression();

	parser->Consume(TokenType::RightParen);

	auto block = new ScopeExpression(parser->parseBlock());
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
	if (parser->LookAhead().type == TokenType::Local)
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
				UniquePtr<Expression*> container = parser->parseExpression();
				parser->Consume(TokenType::RightParen);
				throw 7;
				auto block = new ScopeExpression(parser->parseBlock());
				//return new ForEachExpression(name, container.Release(), block);
			}
		}
	}

	UniquePtr<Expression*> initial = parser->ParseStatement(true);
	UniquePtr<Expression*> condition = parser->ParseStatement(true);
	UniquePtr<Expression*> increment = parser->parseExpression();

	parser->Consume(TokenType::RightParen);

	auto block = new ScopeExpression(parser->parseBlock());
	return new ForExpression(token, initial.Release(), condition.Release(), increment.Release(), block);
}

Expression* SwitchParselet::parse(Parser* parser, Token token)
{
	parser->Consume(TokenType::LeftParen);
	UniquePtr<Expression*> var = parser->parseExpression();
	parser->Consume(TokenType::RightParen);

	BlockExpression* block = parser->parseBlock(false);

	return new SwitchExpression(token, var.Release(), block);
}

Expression* IfParselet::parse(Parser* parser, Token token)
{
	std::vector<Branch*> branches;
	//take parens
	parser->Consume(TokenType::LeftParen);
	UniquePtr<Expression*> ifcondition = parser->parseExpression();
	parser->Consume(TokenType::RightParen);

	BlockExpression* ifblock = parser->parseBlock(true);

	branches.push_back(new Branch(ifblock, ifcondition.Release()));

	Branch* Else = 0;
	while(true)
	{
		//look for elses
		if (parser->MatchAndConsume(TokenType::ElseIf))
		{
			//keep going
			parser->Consume(TokenType::LeftParen);
			UniquePtr<Expression*> condition = parser->parseExpression();
			parser->Consume(TokenType::RightParen);

			BlockExpression* block = parser->parseBlock(true);

			branches.push_back(new Branch(block, condition.Release()));
		}
		else if (parser->MatchAndConsume(TokenType::Else))
		{
			//its an else
			BlockExpression* block = parser->parseBlock(true);

			Else = new Branch(block, 0);
			break;
		}
		else
			break;//nothing else
	}

	return new IfExpression(token, std::move(branches), Else);
}

std::string ParseType(Parser* parser)
{
	Token name = parser->Consume();
	std::string out = name.text;
	while (parser->MatchAndConsume(TokenType::Asterisk))
	{
		//its a pointer
		out += '*';
	}
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
			out += ParseType(parser);
		} while (parser->MatchAndConsume(TokenType::Comma));
		
		parser->Consume(TokenType::GreaterThan);
		out += ">";
	}
	return out;
}

Expression* TraitParselet::parse(Parser* parser, Token token)
{
	Token name = parser->Consume(TokenType::Name);

	parser->Consume(TokenType::LeftBrace);

	//do this later

	parser->Consume(TokenType::RightBrace);

	return new TraitExpression(name);
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
			Token ttname = parser->Consume(TokenType::Name);
			Token tname = parser->Consume(TokenType::Name);

			templated->push_back({ ttname, tname });
		} while (parser->MatchAndConsume(TokenType::Comma));
		parser->Consume(TokenType::GreaterThan);
	}

	parser->Consume(TokenType::LeftBrace);

	auto elements = new std::vector < std::pair<std::string, std::string> > ;
	auto functions = new std::vector < FunctionExpression* > ;
	while (parser->MatchAndConsume(TokenType::RightBrace) == false)
	{
		//first read type
		if (parser->Match(TokenType::Function))
		{
			//parse the function

			auto* expr = parser->ParseStatement(true);

			if (auto fun = dynamic_cast<FunctionExpression*>(expr))
			{
				functions->push_back(fun);
				continue;
			}

			ParserError("Not implemented!", token);
		}

		std::string type = ParseType(parser);

		Token name = parser->Consume(TokenType::Name);//then read name

		if (parser->MatchAndConsume(TokenType::LeftBracket))
		{
			//its an array type
			//read the number
			auto size = parser->parseExpression(Precedence::ASSIGNMENT);

			parser->Consume(TokenType::RightBracket);

			if (auto s = dynamic_cast<NumberExpression*>(size))
			{
				if (s->GetValue() <= 0)
				{
					ParserError("Cannot size array with a zero or negative size", token);
					throw 7;
				}
				type += "[" + std::to_string((int)s->GetValue()) + "]";
			}
			else
			{
				ParserError("Cannot size array with a non constant size", token);
				throw 7;
			}
		}

		parser->Consume(TokenType::Semicolon);		

		elements->push_back({ type, name.text });

		//add member functions!!!
	}
	//done

	return new StructExpression(name, name.text, elements, functions, templated);
}

Expression* FunctionParselet::parse(Parser* parser, Token token)
{
	//read in type
	std::string ret_type = ParseType(parser);

	bool destructor = parser->MatchAndConsume(TokenType::BNot);
	Token name = parser->Consume(TokenType::Name);
	if (destructor)
		name.text = "~" + name.text;
	auto arguments = new std::vector<std::pair<std::string, std::string>>;

	Token stru;
	if (parser->MatchAndConsume(TokenType::Colon))
	{
		parser->Consume(TokenType::Colon);

		//its a struct definition
		stru = name;
		name = parser->Consume(TokenType::Name);//parse the real function name
	}

	NameExpression* varargs = 0;
	parser->Consume(TokenType::LeftParen);

	if (!parser->MatchAndConsume(TokenType::RightParen))
	{
		do
		{
			std::string type = ParseType(parser);

			Token name = parser->Consume();
			if (name.type == TokenType::Name)
			{
				arguments->push_back({type, name.text});// new NameExpression(name.text));
			}
			/*else if (name.type == TokenType::Ellipses)
			{
				varargs = new NameExpression(parser->Consume(TokenType::Name).getText());

				break;//this is end of parsing arguments
			}*/
			else
			{
				std::string str = "Consume: TokenType not as expected! Expected Name or Ellises Got: " + name.text;
				ParserError(str, name);
				//throw CompilerException(parser->filename, name.line, str);
			}
		}
		while(parser->MatchAndConsume(TokenType::Comma));

		parser->Consume(TokenType::RightParen);
	}

	auto block = new ScopeExpression(parser->parseBlock());
	return new FunctionExpression(token, name, ret_type, arguments, block, /*varargs,*/ stru);
}

Expression* ExternParselet::parse(Parser* parser, Token token)
{
	parser->Consume(TokenType::Function);

	std::string ret_type = ParseType(parser);

	Token name = parser->Consume(TokenType::Name);
	auto arguments = new std::vector<std::pair<std::string, std::string>>;

	std::string stru;
	if (parser->MatchAndConsume(TokenType::Colon))
	{
		parser->Consume(TokenType::Colon);

		//its a struct definition
		stru = name.text;
		name = parser->Consume(TokenType::Name);//parse the real function name
	}

	NameExpression* varargs = 0;
	parser->Consume(TokenType::LeftParen);

	if (!parser->MatchAndConsume(TokenType::RightParen))
	{
		do
		{
			std::string type = ParseType(parser);
			Token name = parser->Consume(TokenType::Name);
			if (name.type == TokenType::Name)
			{
				arguments->push_back({ type, name.text });
			}
			/*else if (name.type == TokenType::Ellipses)
			{
				varargs = new NameExpression(parser->Consume(TokenType::Name).getText());

				break;//this is end of parsing arguments
			}*/
			else
			{
				//try and make it handle extra chars better, maybe just parse down to the next ;
				//make this use new error system
				std::string str = "Consume: TokenType not as expected! Expected Name or Ellises Got: " + name.text;
				ParserError(str, name);
				//throw CompilerException(parser->filename, name.line, str);
			}
		} while (parser->MatchAndConsume(TokenType::Comma));

		parser->Consume(TokenType::RightParen);
	}

	return new ExternExpression(token, name, ret_type, arguments, stru);
}


/*Expression* LambdaParselet::parse(Parser* parser, Token token)
{
	parser->Consume(TokenType::LeftParen);

	NameExpression* varargs = 0;
	auto arguments = new std::vector<Expression*>;
	if (parser->LookAhead().type != TokenType::RightParen)
	{
		do
		{
			Token name = parser->Consume();
			if (name.type == TokenType::Name)
			{
				arguments->push_back(new NameExpression(name.getText()));
			}
			else if (name.type == TokenType::Ellipses)
			{
				varargs = new NameExpression(parser->Consume(TokenType::Name).getText());

				break;//this is end of parsing arguments
			}
			else
			{
				std::string str = "Consume: TokenType not as expected! Expected Name or Ellises Got: " + name.text;
				throw CompilerException(parser->filename, name.line, str);
			}
		}
		while(parser->MatchAndConsume(TokenType::Comma));
	}

	parser->Consume(TokenType::RightParen);

	auto block = new ScopeExpression(parser->parseBlock());
	return new FunctionExpression(token, 0, arguments, block, varargs);
}*/

Expression* CallParselet::parse(Parser* parser, Expression* left, Token token)
{
	UniquePtr<std::vector<Expression*>*> arguments = new std::vector<Expression*>;

	if (!parser->MatchAndConsume(TokenType::RightParen))
	{
		do
		{
			arguments->push_back(parser->parseExpression(Precedence::ASSIGNMENT));
		}
		while( parser->MatchAndConsume(TokenType::Comma));

		parser->Consume(TokenType::RightParen);
	}
	return new CallExpression(token, left, arguments.Release());
}

Expression* ReturnParselet::parse(Parser* parser, Token token)
{
	Expression* right = 0;
	if (parser->Match(TokenType::Semicolon) == false)
		right = parser->parseExpression(Precedence::ASSIGNMENT);

	return new ReturnExpression(token, right);
}

Expression* LocalParselet::parse(Parser* parser, Token token)
{
	UniquePtr<std::vector<std::pair<std::string, Token>>*> names = new std::vector<std::pair<std::string, Token>>;

	do
	{
		auto next = parser->LookAhead(1);
		std::string type = next.type == TokenType::Assign ? "" : ParseType(parser);
		
		Token name = parser->Consume(TokenType::Name);

		if (parser->MatchAndConsume(TokenType::LeftBracket))
		{
			//its an array type
			//read the number
			auto size = parser->parseExpression(Precedence::ASSIGNMENT);

			parser->Consume(TokenType::RightBracket);

			if (auto s = dynamic_cast<NumberExpression*>(size))
			{
				if (s->GetValue() <= 0)
				{
					ParserError("Cannot size array with a zero or negative size", token);
					throw 7;
				}
				type += "[" + std::to_string((int)s->GetValue()) + "]";
			}
			else
			{
				ParserError("Cannot size array with a non constant size", token);
				throw 7;
			}
		}
		names->push_back({ type, name });
	}
	while (parser->MatchAndConsume(TokenType::Comma));

	if (parser->MatchAndConsume(TokenType::Semicolon))
		return new LocalExpression(names.Release(), 0);

	parser->Consume(TokenType::Assign);//its possible this wont be here and it may just be a mentioning, but no assignment

	//handle multiple comma expressions
	UniquePtr<std::vector<Expression*>*> rights = new std::vector<Expression*>;
	do
	{
		Expression* right = parser->parseExpression(Precedence::ASSIGNMENT-1/*assignment prcedence -1 */);

		rights->push_back(right);
	}
	while (parser->MatchAndConsume(TokenType::Comma));

	parser->Consume(TokenType::Semicolon);

	return new LocalExpression(names.Release(), rights.Release());
}

Expression* ConstParselet::parse(Parser* parser, Token token)
{
	ParserError("Not Implemented", token);
	//throw CompilerException("", 0, "Const keyword not implemented!");

	auto names = new std::vector<std::pair<std::string,Token>>;
	do
	{
		Token name = parser->Consume(TokenType::Name);
		//names->push_back(name);
	}
	while (parser->MatchAndConsume(TokenType::Comma));

	parser->Consume(TokenType::Assign);//its possible this wont be here and it may just be a mentioning, but no assignment

	//do somethign with multiple comma expressions
	std::vector<Expression*>* rights = new std::vector<Expression*>;
	do
	{
		Expression* right = parser->parseExpression(Precedence::ASSIGNMENT-1/*assignment prcedence -1 */);

		rights->push_back(right);
	}
	while (parser->MatchAndConsume(TokenType::Comma));

	parser->Consume(TokenType::Semicolon);
	//do stuff with this and store and what not
	//need to add this variable to this's block expression

	return new LocalExpression(names, rights);
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
	UniquePtr<Expression*> index = parser->parseExpression();
	parser->Consume(TokenType::RightBracket);

	return new IndexExpression(left, index.Release(), token);
}

Expression* MemberParselet::parse(Parser* parser, Expression* left, Token token)
{
	//this is for const members
	Expression* member = parser->parseExpression(Precedence::CALL);
	UniquePtr<NameExpression*> name = dynamic_cast<NameExpression*>(member);
	if (name == 0)
	{
		delete member; delete left;
		ParserError("Cannot access member name that is not a string", token);
		//throw CompilerException(parser->filename, token.line, "Cannot access member name that is not a string");
	}

	auto ret = new IndexExpression(left, new StringExpression(name->GetName()), token);

	return ret;
}

Expression* PointerMemberParselet::parse(Parser* parser, Expression* left, Token token)
{
	//this is for const members
	Expression* member = parser->parseExpression(Precedence::CALL);
	UniquePtr<NameExpression*> name = dynamic_cast<NameExpression*>(member);
	if (name == 0)
	{
		delete member; delete left;
		ParserError("Cannot access member name that is not a string", token);
		//throw CompilerException(parser->filename, token.line, "Cannot access member name that is not a string");
	}

	auto ret = new IndexExpression(left, name->GetName(), token);

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
};

Expression* YieldParselet::parse(Parser* parser, Token token)
{
	Expression* right = 0;
	if (parser->Match(TokenType::Semicolon) == false)
		right = parser->parseExpression(Precedence::ASSIGNMENT);

	return new YieldExpression(token, right);
}

Expression* InlineYieldParselet::parse(Parser* parser, Token token)
{
	Expression* right = 0;
	if (parser->Match(TokenType::Semicolon) == false && parser->LookAhead().type != TokenType::RightParen)
		right = parser->parseExpression(Precedence::ASSIGNMENT);

	return new YieldExpression(token, right);
}

Expression* ResumeParselet::parse(Parser* parser, Token token)
{
	Expression* right = parser->parseExpression(Precedence::ASSIGNMENT);

	return new ResumeExpression(token, right);
}

Expression* ResumePrefixParselet::parse(Parser* parser, Token token)
{
	Expression* right = parser->parseExpression(Precedence::ASSIGNMENT);

	return new ResumeExpression(token, right);
}*/