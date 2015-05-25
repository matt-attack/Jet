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

		//newkeywords
		Extern,
		Struct,

		Const,

		Swap,

		Ret,

		Increment,
		Decrement,

		Operator,

		LineComment,
		CommentBegin,
		CommentEnd,

		EoF
	};

	char* Operator(TokenType t);

	struct Token
	{
		TokenType type;
		std::string text;
		unsigned int line;

		Token()
		{

		}

		Token(unsigned int line, TokenType type, std::string txt)
		{
			this->type = type;
			this->text = txt;
			this->line = line;
		}

		TokenType getType()
		{
			return type;
		}

		std::string getText()
		{
			return text;
		}
	};
}

#endif