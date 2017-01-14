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
		Not,
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
		//DoubleBracket,

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
		Let,
		Break,
		Continue,
		Yield,
		Resume,

		Match,

		Switch,
		Case,
		Default,

		//newkeywords
		Extern,
		Struct,
		Trait,
		Namespace,
		Union,
		Enum,

		Generator,

		Scope,//::

		Const,

		Swap,

		Ret,

		Increment,
		Decrement,

		Pointy, 

		Operator,

		LineComment,
		CommentBegin,
		CommentEnd,

		SizeOf,
		OffsetOf,
		TypeOf,
		New,
		Free,

		Typedef, 

		//special token for !<
		TemplateBegin,

		EoF,

		IfMacro,
		ElseMacro,
		ElseIfMacro,
		EndIfMacro,

		InvalidToken
	};

	struct Range
	{
		unsigned int index;
		unsigned int length;
	};

	class Source;
	class Compilation;
	struct Token
	{
		TokenType type;
		std::string text;
		
		//Range text;
		const char* text_ptr;
		int trivia_length;//length of preceding whitespace/comments

		unsigned int line;
		unsigned int column;

		Token()
		{
			line = 1;
			type = TokenType::InvalidToken;
			trivia_length = 0;
		}

		Token(const char* source, unsigned int trivia_length, unsigned int line, unsigned int column, TokenType type, std::string txt)
		{
			this->text_ptr = source;
			//this->text_ptr = txtptr;
			this->trivia_length = trivia_length;
			this->type = type;
			this->text = txt;
			this->line = line;
			this->column = column;
		}

		void Print(std::string& str, Source* source) const;

		Source* GetSource(Compilation* compilation) const;
	};

	struct TokenRange
	{
		Token first;
		Token last;
	};
}

#endif