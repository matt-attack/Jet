#include "Parser.h"
#include "Token.h"
#include "UniquePtr.h"

using namespace Jet;

Parser::Parser(Lexer* l)
{
	this->lexer = l;
	//this->filename = l->filename;

	this->Register(TokenType::Name, new NameParselet());
	this->Register(TokenType::Number, new NumberParselet());
	this->Register(TokenType::String, new StringParselet());
	this->Register(TokenType::Assign, new AssignParselet());

	this->Register(TokenType::LeftParen, new GroupParselet());

	//this->Register(TokenType::Swap, new SwapParselet());

	//this->Register(TokenType::Colon, new MemberParselet());
	this->Register(TokenType::Dot, new MemberParselet());
	//this->Register(TokenType::LeftBrace, new ObjectParselet());

	//array/index stuffs
	//this->Register(TokenType::LeftBracket, new ArrayParselet());
	this->Register(TokenType::LeftBracket, new IndexParselet());//postfix

	//operator assign
	this->Register(TokenType::AddAssign, new OperatorAssignParselet());
	this->Register(TokenType::SubtractAssign, new OperatorAssignParselet());
	this->Register(TokenType::MultiplyAssign, new OperatorAssignParselet());
	this->Register(TokenType::DivideAssign, new OperatorAssignParselet());
	this->Register(TokenType::AndAssign, new OperatorAssignParselet());
	this->Register(TokenType::OrAssign, new OperatorAssignParselet());
	this->Register(TokenType::XorAssign, new OperatorAssignParselet());


	//prefix stuff
	this->Register(TokenType::Increment, new PrefixOperatorParselet(Precedence::PREFIX));
	this->Register(TokenType::Decrement, new PrefixOperatorParselet(Precedence::PREFIX));
	this->Register(TokenType::Minus, new PrefixOperatorParselet(Precedence::PREFIX));
	this->Register(TokenType::BNot, new PrefixOperatorParselet(Precedence::PREFIX));
	this->Register(TokenType::BAnd, new PrefixOperatorParselet(Precedence::PREFIX));
	this->Register(TokenType::Asterisk, new PrefixOperatorParselet(Precedence::PREFIX));

	//postfix stuff
	this->Register(TokenType::Increment, new PostfixOperatorParselet(Precedence::POSTFIX));
	this->Register(TokenType::Decrement, new PostfixOperatorParselet(Precedence::POSTFIX));

	//boolean stuff
	this->Register(TokenType::Equals, new BinaryOperatorParselet(Precedence::CONDITIONAL, false));
	this->Register(TokenType::NotEqual, new BinaryOperatorParselet(Precedence::CONDITIONAL, false));
	this->Register(TokenType::LessThan, new BinaryOperatorParselet(Precedence::CONDITIONAL, false));
	this->Register(TokenType::GreaterThan, new BinaryOperatorParselet(Precedence::CONDITIONAL, false));
	this->Register(TokenType::LessThanEqual, new BinaryOperatorParselet(Precedence::CONDITIONAL, false));
	this->Register(TokenType::GreaterThanEqual, new BinaryOperatorParselet(Precedence::CONDITIONAL, false));

	//logical and/or
	this->Register(TokenType::And, new BinaryOperatorParselet(Precedence::LOGICAL, false));
	this->Register(TokenType::Or, new BinaryOperatorParselet(Precedence::LOGICAL, false));

	//math
	this->Register(TokenType::Plus, new BinaryOperatorParselet(Precedence::SUM, false));
	this->Register(TokenType::Minus, new BinaryOperatorParselet(Precedence::SUM, false));
	this->Register(TokenType::Asterisk, new BinaryOperatorParselet(Precedence::PRODUCT, false));
	this->Register(TokenType::Slash, new BinaryOperatorParselet(Precedence::PRODUCT, false));
	this->Register(TokenType::Modulo, new BinaryOperatorParselet(Precedence::PRODUCT, false));
	this->Register(TokenType::BOr, new BinaryOperatorParselet(Precedence::BINARY, false));//or
	this->Register(TokenType::BAnd, new BinaryOperatorParselet(Precedence::BINARY, false));//and
	this->Register(TokenType::Xor, new BinaryOperatorParselet(Precedence::BINARY, false));
	this->Register(TokenType::LeftShift, new BinaryOperatorParselet(Precedence::BINARY, false));
	this->Register(TokenType::RightShift, new BinaryOperatorParselet(Precedence::BINARY, false));

	//add parser for includes k
	//function stuff
	this->Register(TokenType::LeftParen, new CallParselet());

	//lambda
	//this->Register(TokenType::Function, new LambdaParselet());
	//this->Register(TokenType::LeftParen, new LambdaParselet());

	//statements
	this->Register(TokenType::While, new WhileParselet());
	this->Register(TokenType::If, new IfParselet());
	this->Register(TokenType::Function, new FunctionParselet());
	this->Register(TokenType::Ret, new ReturnParselet());
	this->Register(TokenType::For, new ForParselet());

	this->Register(TokenType::Switch, new SwitchParselet());
	this->Register(TokenType::Case, new CaseParselet());
	this->Register(TokenType::Default, new DefaultParselet());

	this->Register(TokenType::Local, new LocalParselet());

	this->Register(TokenType::Extern, new ExternParselet());
	this->Register(TokenType::Struct, new StructParselet());
	this->Register(TokenType::Trait, new TraitParselet());

	this->Register(TokenType::Break, new BreakParselet());
	this->Register(TokenType::Continue, new ContinueParselet());

	//this->Register(TokenType::Const, new ConstParselet());
	//this->Register(TokenType::Null, new NullParselet());

	//this->Register(TokenType::Yield, new YieldParselet());
	//this->Register(TokenType::Yield, new InlineYieldParselet());
	//this->Register(TokenType::Resume, new ResumeParselet());
	//this->Register(TokenType::Resume, new ResumePrefixParselet());
}

Parser::~Parser()
{
	for (auto ii : this->mInfixParselets)
		delete ii.second;

	for (auto ii : this->mPrefixParselets)
		delete ii.second;

	for (auto ii : this->mStatementParselets)
		delete ii.second;
};

Expression* Parser::parseExpression(int precedence)
{
	Token token = Consume();
	PrefixParselet* prefix = mPrefixParselets[token.type];

	if (prefix == 0)
	{
		std::string str = "ParseExpression: No Parser Found for: " + token.text;
		//throw CompilerException(this->filename, token.line, str);//printf("Consume: TokenType not as expected!\n");
		ParserError(str, token);
		return 0;
	}

	Expression* left = prefix->parse(this, token);
	while (precedence < getPrecedence())
	{
		token = Consume();

		InfixParselet* infix = mInfixParselets[token.type];
		left = infix->parse(this, left, token);
	}
	return left;
}

Expression* Parser::ParseStatement(bool takeTrailingSemicolon)//call this until out of tokens (hit EOF)
{
	Token token = LookAhead();
	StatementParselet* statement = mStatementParselets[token.type];

	if (statement == 0)
	{
		UniquePtr<Expression*> result(parseExpression());

		if (takeTrailingSemicolon)
			Consume(TokenType::Semicolon);

		return result.Release();
	}

	token = Consume();
	UniquePtr<Expression*> result(statement->parse(this, token));

	if (takeTrailingSemicolon && statement->TrailingSemicolon)
		Consume(TokenType::Semicolon);

	return result.Release();
}

BlockExpression* Parser::parseBlock(bool allowsingle)
{
	std::vector<Expression*> statements;

	if (allowsingle && !Match(TokenType::LeftBrace))
	{
		auto res = this->ParseStatement();
		if (res)
		statements.push_back(res);
		return new BlockExpression(std::move(statements));
	}

	Consume(TokenType::LeftBrace);

	while (!Match(TokenType::RightBrace))
	{
		auto res = this->ParseStatement();
		if (res)
			statements.push_back(res);
	}

	Consume(TokenType::RightBrace);
	return new BlockExpression(std::move(statements));
}

BlockExpression* Parser::parseAll()
{
	std::vector<Expression*> statements;
	while (!Match(TokenType::EoF))
	{
		auto res = this->ParseStatement();
		if (res)
			statements.push_back(res);
	}
	auto n = new BlockExpression(std::move(statements));
	n->SetParent(0);//go through and setup parents
	return n;
}

Token Parser::Consume()
{
	auto temp = LookAhead();
	mRead.pop_front();
	return temp;
}

Token Parser::Consume(TokenType expected)
{
	auto temp = LookAhead();
	if (temp.type != expected)
	{
		std::string str = "Consume: TokenType Not As Expected! Expected: " + TokenToString[expected] + " Got: " + temp.text;
		//throw CompilerException(this->filename, temp.line, str);

		//fabricate a fake token
		ParserError(str, temp);
		//lets give up on this, it doesnt work well
		throw 7;

		mRead.pop_front();
		return Token(temp.line, temp.column, expected, "uh");
	}
	mRead.pop_front();
	return temp;
}

Token Parser::LookAhead(unsigned int num)
{
	while (num >= mRead.size())
		mRead.push_back(lexer->Next());

	int c = 0;
	for (auto ii : mRead)
	{
		if (c++ == num)
			return ii;
	}

	return Token(0, 0, TokenType::EoF, "EOF");
}

bool Parser::Match(TokenType expected)
{
	Token token = LookAhead();
	if (token.type != expected)
	{
		return false;
	}

	return true;
}

bool Parser::MatchAndConsume(TokenType expected)
{
	Token token = LookAhead();
	if (token.type != expected)
	{
		return false;
	}

	mRead.pop_front();
	return true;
}

void Parser::Register(TokenType token, InfixParselet* parselet)
{
	this->mInfixParselets[token] = parselet;
}

void Parser::Register(TokenType token, PrefixParselet* parselet)
{
	this->mPrefixParselets[token] = parselet;
}

void Parser::Register(TokenType token, StatementParselet* parselet)
{
	this->mStatementParselets[token] = parselet;
}

int Parser::getPrecedence() {
	InfixParselet* parser = mInfixParselets[LookAhead(0).type];
	if (parser != 0)
		return parser->getPrecedence();

	return 0;
}