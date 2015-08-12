#include "Lexer.h"
#include "Parser.h"
#include "Source.h"

using namespace Jet;

std::map<TokenType, std::string> Jet::TokenToString;

class LexerStatic
{
public:
	std::map<std::string, TokenType> operators;
	std::map<std::string, TokenType> keywords;
	std::map<char, std::vector<std::pair<std::string, TokenType>>> operatorsearch;

	LexerStatic()
	{
		//math and assignment
		operators["="] = TokenType::Assign;
		operators["+"] = TokenType::Plus;
		operators["-"] = TokenType::Minus;
		operators["*"] = TokenType::Asterisk;
		operators["/"] = TokenType::Slash;
		operators["%"] = TokenType::Modulo;

		operators["&&"] = TokenType::And;
		operators["||"] = TokenType::Or;

		operators["|"] = TokenType::BOr;//or
		operators["&"] = TokenType::BAnd;//and
		operators["^"] = TokenType::Xor;//xor
		operators["~"] = TokenType::BNot;//binary not
		operators["<<"] = TokenType::LeftShift;
		operators[">>"] = TokenType::RightShift;

		//grouping
		operators["("] = TokenType::LeftParen;
		operators[")"] = TokenType::RightParen;
		operators["{"] = TokenType::LeftBrace;
		operators["}"] = TokenType::RightBrace;

		//array stuff
		operators["["] = TokenType::LeftBracket;
		operators["]"] = TokenType::RightBracket;

		//object stuff
		operators["."] = TokenType::Dot;
		operators[":"] = TokenType::Colon;
		operators[";"] = TokenType::Semicolon;
		operators[","] = TokenType::Comma;

		operators["++"] = TokenType::Increment;
		operators["--"] = TokenType::Decrement;

		//operator + equals sign
		operators["+="] = TokenType::AddAssign;
		operators["-="] = TokenType::SubtractAssign;
		operators["*="] = TokenType::MultiplyAssign;
		operators["/="] = TokenType::DivideAssign;
		operators["&="] = TokenType::AndAssign;
		operators["|="] = TokenType::OrAssign;
		operators["^="] = TokenType::XorAssign;

		//boolean logic
		operators["!="] = TokenType::NotEqual;
		operators["=="] = TokenType::Equals;

		//comparisons
		operators["<"] = TokenType::LessThan;
		operators[">"] = TokenType::GreaterThan;
		operators["<="] = TokenType::LessThanEqual;
		operators[">="] = TokenType::GreaterThanEqual;

		//special stuff
		operators["<>"] = TokenType::Swap;
		operators["\""] = TokenType::String;

		operators["..."] = TokenType::Ellipses;

		//comments
		operators["//"] = TokenType::LineComment;
		operators["-[["] = TokenType::BlockString;
		//operators["]]-"] = TokenType::CommentEnd;
		operators["/*"] = TokenType::CommentBegin;
		operators["*/"] = TokenType::CommentEnd;

		//dereference member
		operators["->"] = TokenType::Pointy;

		//keywords
		keywords["while"] = TokenType::While;
		keywords["mientras"] = TokenType::While;
		keywords["if"] = TokenType::If;
		keywords["si"] = TokenType::If;
		keywords["elseif"] = TokenType::ElseIf;
		keywords["otrosi"] = TokenType::ElseIf;
		keywords["else"] = TokenType::Else;
		keywords["otro"] = TokenType::Else;
		keywords["fun"] = TokenType::Function;
		keywords["return"] = TokenType::Ret;
		keywords["volver"] = TokenType::Ret;
		keywords["for"] = TokenType::For;
		//add spanish mode yo!
		keywords["por"] = TokenType::For;
		keywords["local"] = TokenType::Local;
		keywords["break"] = TokenType::Break;
		keywords["romper"/*"parar"*/] = TokenType::Break;
		keywords["continue"] = TokenType::Continue;
		keywords["continuar"] = TokenType::Continue;

		keywords["null"] = TokenType::Null;

		keywords["yield"] = TokenType::Yield;
		keywords["resume"] = TokenType::Resume;

		keywords["switch"] = TokenType::Switch;
		keywords["case"] = TokenType::Case;
		keywords["default"] = TokenType::Default;

		keywords["trait"] = TokenType::Trait;


		keywords["extern"] = TokenType::Extern;
		keywords["struct"] = TokenType::Struct;
		keywords["namespace"] = TokenType::Namespace;

		//internal "functions"
		keywords["sizeof"] = TokenType::SizeOf;
		keywords["offsetof"] = TokenType::OffsetOf;

		keywords["typedef"] = TokenType::Typedef;
		//keywords["const"] = TokenType::Const;

		//keywords["operator"] = TokenType::Operator;

		for (auto ii = operators.begin(); ii != operators.end(); ii++)
		{
			TokenToString[ii->second] = ii->first;

			//build search structure
			auto t = operatorsearch.find(ii->first[0]);
			if (t != operatorsearch.end())
			{
				t->second.push_back(std::pair<std::string, TokenType>(ii->first, ii->second));
			}
			else
			{
				operatorsearch[ii->first[0]] = std::vector<std::pair<std::string, TokenType>>();
				operatorsearch[ii->first[0]].push_back(std::pair<std::string, TokenType>(ii->first, ii->second));
			}
		}

		for (auto ii : keywords)
		{
			TokenToString[ii.second] = ii.first;
		}

		TokenToString[TokenType::Name] = "name";
	};
};


bool Jet::IsLetter(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool Jet::IsNumber(char c)
{
	return (c >= '0' && c <= '9');
}

Lexer::Lexer(Source* source)// std::istream* input, std::string filename)
{
	this->last_index = 0;
	this->src = source;
	//this->stream = input;
	//this->linenumber = 1;
	//this->column = 0;
	//this->index = 0;
	//this->filename = filename;
}

/*Lexer::Lexer(std::string text, std::string filename)
{
//this->stream = 0;
//this->column = 0;
//this->linenumber = 1;
//this->index = 0;
//this->text = text;
this->filename = filename;
}*/



//can move to functions at some point
void ParseKeyword(const std::string& string)
{

}

Token Lexer::Next()
{
	static LexerStatic ls;
	while (src->IsAtEnd() == false)
	{
		int trivia_length = src->GetIndex() - this->last_index;
		const char* t_ptr = src->GetSubstring(src->GetIndex(), 0);
		int column = src->column;

		char c = src->ConsumeChar();
		std::string str;
		str += c;// = text.substr(index - 1, 1);
		bool found = false; unsigned int len = 0;
		TokenType toktype;
		auto iter = ls.operatorsearch.find(str[0]);
		if (iter != ls.operatorsearch.end())
		{
			for (auto ii : iter->second)
			{
				if (ii.first.length() > src->Remaining())///(text.length() + 1 - index))
					continue;

				//pick the longest matching operator/keyword
				if (ii.first.length() <= len)
					continue;

				//check if the characters match the operator/keyword
				if (src->IsOperator(ii.first))// memcmp(ii.first.c_str(), &text.c_str()[index - 1], ii.first.length()) == 0)
				{
					len = ii.first.length();
					str = ii.first;
					toktype = ii.second;
					found = true;
				}
			}
		}
		else
		{
			found = false;
		}

		if (found)
		{
			for (unsigned int i = 0; i < len - 1; i++)
				src->ConsumeChar();

			//remove these use of operators
			if (toktype == TokenType::LineComment)
			{
				//go to next line
				char c = src->ConsumeChar();
				while (c != '\n' && c != 0)
				{
					c = src->ConsumeChar();
				}

				if (c == 0)
					break;

				continue;
			}
			else if (toktype == TokenType::CommentBegin)
			{
				int startline = src->linenumber;
				//Token n = this->Next();
				while (true)
				{
					char c = src->ConsumeChar();
					if (c == '*' && src->PeekChar() == '/')
					{
						src->ConsumeChar();
						break;
					}
					else if (c == 0)
						ParserError("Missing end to comment block starting at line " + std::to_string(startline), Token(t_ptr, 0, src->linenumber, column, TokenType::String, ""));

					//throw CompilerException(this->filename, this->linenumber, "Missing end to comment block starting at line " + std::to_string(startline));
				}

				continue;
			}
			else if (toktype == TokenType::String)
			{
				std::string txt;

				//int start = index;
				while (src->IsAtEnd() == false)//index < text.length())
				{
					char p = src->PeekChar();
					if (p == '\\')//text[index] == '\\')
					{
						src->ConsumeChar();
						//handle escape sequences
						char c = src->ConsumeChar();// text[index + 1];
						switch (c)
						{
						case 'n':
							txt.push_back('\n');
							break;
						case 'b':
							txt.push_back('\b');
							break;
						case 't':
							txt.push_back('\t');
							break;
						case '\\':
							txt.push_back('\\');
							break;
						case '"':
							txt.push_back('"');
							break;
						case '0':
							txt.push_back(0);
							break;
						default:
							std::string ch;
							ch += c;
							ParserError("Invalid Escape Sequence '\\" + ch/*text.substr(index - 1, 1)*/ + "'", Token(t_ptr, 0, src->linenumber, src->column-2, TokenType::String, std::string("\\")+ch));
							//ParserError("Invalid Escape Sequence '\\" + std::to_string(c) + "'", Token(t_ptr, 0, src->linenumber, column, TokenType::String, ""));

							//throw CompilerException(filename, this->linenumber, "Invalid Escape Sequence '\\" + text.substr(index + 1, 1) + "'");
						}

						//index += 2;
					}
					else if (p == '"')
					{
						src->ConsumeChar();
						break;
					}
					else
					{
						txt.push_back(src->ConsumeChar());// text[index++]);
					}
				}
				this->last_index = src->GetIndex();

				//index++;
				return Token(t_ptr, trivia_length, src->linenumber, column, toktype, txt);
			}
			else if (toktype == TokenType::BlockString)
			{
				ParserError("Block Strings Not Implemented", Token(t_ptr, 0, src->linenumber, column, TokenType::String, ""));

				/*std::string txt;

				int start = index;
				while (src->IsAtEnd() == false)//index < text.length())
				{
				if (text[index] == '\\')
				{
				//handle escape sequences
				char c = text[index + 1];
				switch (c)
				{
				case 'n':
				txt.push_back('\n');
				break;
				case 'b':
				txt.push_back('\b');
				break;
				case 't':
				txt.push_back('\t');
				break;
				case '\\':
				txt.push_back('\\');
				break;
				case '"':
				txt.push_back('"');
				break;
				default:
				ParserError("Invalid Escape Sequence '\\" + text.substr(index + 1, 1) + "'", Token(src->linenumber, src->column, TokenType::String, ""));

				//throw CompilerException(filename, this->linenumber, "Invalid Escape Sequence '\\" + text.substr(index + 1, 1) + "'");
				}

				index += 2;
				}
				else if (text[index] == ']' && text[index + 1] == ']' && text[index + 2] == '-')
				{
				break;
				}
				else
				{
				txt.push_back(text[index++]);
				}
				}

				index += 3;
				return Token(src->linenumber, src->column, TokenType::String, txt);*/
			}
			this->last_index = src->GetIndex();

			return Token(t_ptr, trivia_length, src->linenumber, column, toktype, str);
		}
		else if (IsLetter(c) || c == '_')//word
		{
			//int start = index - 1;
			std::string name;
			name += c;
			while (true)
			{
				char c = src->PeekChar();
				if (!(IsLetter(c) || c == '_'))
					if (!IsNumber(c))
						break;

				name += src->ConsumeChar();
			}

			this->last_index = src->GetIndex();

			//std::string name = text.substr(start, index - start);
			//check if it is a keyword
			auto keyword = ls.keywords.find(name);
			if (keyword != ls.keywords.end())//is keyword?
				return Token(t_ptr, trivia_length, src->linenumber, column, keyword->second, name);
			else//just a variable name
				return Token(t_ptr, trivia_length, src->linenumber, column, TokenType::Name, name);
		}
		else if (IsNumber(c))//number
		{
			//finish adding different literal types
			//int start = index - 1;
			if (c != '0')
			{
				std::string num;
				num += c;
				while (true)
				{
					char c = src->PeekChar();
					if (!(c == '.' || IsNumber(c)))
						break;

					num += src->ConsumeChar();
				}

				this->last_index = src->GetIndex();

				//std::string num = text.substr(start, index - start);
				return Token(t_ptr, trivia_length, src->linenumber, column, TokenType::Number, num);
			}
			else
			{
				//ok, its possibly some kind of non base ten literal
				char c = src->PeekChar();
				switch (c)
				{
				case 'b':
				{
					src->ConsumeChar();
					std::string num;
					while (true)
					{
						char c = src->PeekChar();
						if (!(c == '.' || IsNumber(c)))
							break;

						num += src->ConsumeChar();
					}
					this->last_index = src->GetIndex();

					//std::string num = text.substr(start, index - start);
					return Token(t_ptr, trivia_length, src->linenumber, column, TokenType::Number, num);
					break;
				}
				case 'x'://hex literal
				{
					src->ConsumeChar();
					std::string num;
					while (true)
					{
						char c = src->PeekChar();
						if (!(c == '.' || IsNumber(c) || (c <= 'f' && c >= 'a') || (c <= 'F' && c >= 'A')))
							break;

						num += src->ConsumeChar();
					}
					this->last_index = src->GetIndex();

					//std::string num = text.substr(start, index - start);
					return Token(t_ptr, trivia_length, src->linenumber, column, TokenType::Number, num);
					break;
				}
				default:
				{
					while (true)
					{
						char c = src->PeekChar();
						if (!(c == '.' || IsNumber(c)))
							break;

						str += src->ConsumeChar();
					}
					this->last_index = src->GetIndex();

					//std::string num = text.substr(start, index - start);
					return Token(t_ptr, trivia_length, src->linenumber, column, TokenType::Number, str);
				}
				}
			}
		}
		else if (c == '\'')
		{
			char cc = src->ConsumeChar();
			if (cc == '\\')
			{
				//handle the escape sequence
				cc = src->ConsumeChar();
				switch (cc)
				{
				case 'n':
					cc = '\n';
					break;
				case 'b':
					cc = '\b';
					break;
				case 't':
					cc = '\t';
					break;
				case '\\':
					cc = '\\';
					break;
				case '\'':
					cc = '\'';
					break;
				default:
				{
					std::string ch;
					ch += cc;
					ParserError("Invalid Escape Sequence '\\" + ch/*text.substr(index - 1, 1)*/ + "'", Token(t_ptr, 0, src->linenumber, src->column - 2, TokenType::String, std::string("\\") + ch));
				}
				}
			}
			std::string num = std::to_string((int)cc);

			char endc = src->ConsumeChar();
			if (endc != '\'')
				ParserError("Closing ' expected for character literal.", Token(t_ptr, 0, src->linenumber, column, TokenType::String, ""));

			this->last_index = src->GetIndex();

			//	throw CompilerException(filename, linenumber, "Closing ' expected for character literal.");
			return Token(t_ptr, trivia_length, src->linenumber, column, TokenType::Number, num);
		}
		else
		{
			//character to ignore like whitespace
			if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
				continue;
			else
				ParserError("Unexpected character: '" + str + "'", Token(t_ptr, 0, src->linenumber, column, TokenType::String, ""));
			//throw CompilerException(this->filename, this->linenumber, "Unexpected character: '" + str + "'");
		}
	}
	this->last_index = src->GetIndex();

	return Token(0, 0, src->linenumber, src->column, TokenType::EoF, "EOF");
}

