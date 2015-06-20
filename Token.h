#ifndef _TOKEN_HEADER
#define _TOKEN_HEADER

#include <string>

namespace Jet
{
	enum class TokenType
	{
		Name,
		Number,
		String,
		BlockString,
		Assign,

		Dot,

		Minus,
		Plus,
		Asterisk,
		Slash,
		Modulo,
		Or,
		And,
		BOr,
		BAnd,
		Xor,
		BNot,
		LeftShift,
		RightShift,


		AddAssign,
		SubtractAssign,
		MultiplyAssign,
		DivideAssign,
		AndAssign,
		OrAssign,
		XorAssign,

		NotEqual,
		Equals,

		LessThan,
		GreaterThan,
		LessThanEqual,
		GreaterThanEqual,

		RightParen,
		LeftParen,

		LeftBrace,
		RightBrace,

		LeftBracket,
		RightBracket,

		While,
		If,
		ElseIf,
		Else,

		Colon,
		Semicolon,
		Comma,
		Ellipses,

		Null,

		Function,
		For,
		Local,
		Break,
		Continue,
		Yield,
		Resume,

		Switch,
		Case,
		Default,

		//newkeywords
		Extern,
		Struct,
		Trait,

		Const,

		Swap,

		Ret,

		Increment,
		Decrement,

		//Dereference,
		//AddressOf,

		Pointy, 

		Operator,

		LineComment,
		CommentBegin,
		CommentEnd,

		EoF
	};

	struct Range
	{
		unsigned int index;
		unsigned int length;
	};
	struct Token
	{
		TokenType type;
		std::string text;
		
		//Range text;

		unsigned int line;
		unsigned int column;

		Token()
		{

		}

		Token(unsigned int line, unsigned int column, TokenType type, std::string txt)
		{
			this->type = type;
			this->text = txt;
			this->line = line;
			this->column = column;
		}
	};
}

#endif