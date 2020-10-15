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

		operators["!"] = TokenType::Not;

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
		//operators["[]"] = TokenType::DoubleBracket;

		//object stuff
		operators["."] = TokenType::Dot;
		operators[":"] = TokenType::Colon;
		operators[";"] = TokenType::Semicolon;
		operators[","] = TokenType::Comma;

		operators["::"] = TokenType::Scope;

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

		operators["!<"] = TokenType::TemplateBegin;

		//keywords
		keywords["while"] = TokenType::While;
		keywords["if"] = TokenType::If;
		keywords["elseif"] = TokenType::ElseIf;
		keywords["else"] = TokenType::Else;
		keywords["fun"] = TokenType::Function;
		keywords["virtual"] = TokenType::Virtual;
		keywords["return"] = TokenType::Ret;
		keywords["for"] = TokenType::For;

		keywords["let"] = TokenType::Let;
		keywords["const"] = TokenType::Const;
		keywords["break"] = TokenType::Break;
		keywords["continue"] = TokenType::Continue;

		keywords["null"] = TokenType::Null;

		keywords["yield"] = TokenType::Yield;
		keywords["generator"] = TokenType::Generator;
		keywords["resume"] = TokenType::Resume;

		keywords["match"] = TokenType::Match;

		keywords["switch"] = TokenType::Switch;
		keywords["case"] = TokenType::Case;
		keywords["default"] = TokenType::Default;

		keywords["trait"] = TokenType::Trait;

		keywords["union"] = TokenType::Union;
		keywords["extern"] = TokenType::Extern;
		keywords["extern_c"] = TokenType::Extern;
		keywords["struct"] = TokenType::Struct;
		keywords["class"] = TokenType::Class;
		keywords["namespace"] = TokenType::Namespace;
		keywords["enum"] = TokenType::Enum;

		//internal "functions"
		keywords["sizeof"] = TokenType::SizeOf;
		keywords["offsetof"] = TokenType::OffsetOf;
		keywords["typeof"] = TokenType::TypeOf;
		keywords["new"] = TokenType::New;
		keywords["free"] = TokenType::Free;

		keywords["typedef"] = TokenType::Typedef;

		operators["#if"] = TokenType::IfMacro;
		operators["#else"] = TokenType::ElseMacro;
		operators["#elseif"] = TokenType::ElseIfMacro;
		operators["#endif"] = TokenType::EndIfMacro;

		operators["//!@!"] = TokenType::LocationMacro;
		

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
			TokenToString[ii.second] = ii.first;

		TokenToString[TokenType::Name] = "name";
	}
};

bool Jet::IsLetter(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool Jet::IsNumber(char c)
{
	return (c >= '0' && c <= '9');
}

Lexer::Lexer(Source* source, DiagnosticBuilder* diag, const std::map<std::string, bool>& defines)
{
	this->diag = diag;
	this->last_index = 0;
	this->src = source;
	this->defines = defines;
}


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
		str += c;
		bool found = false; unsigned int len = 0;
		TokenType toktype;
		auto iter = ls.operatorsearch.find(str[0]);
		if (iter != ls.operatorsearch.end())
		{
			for (auto ii : iter->second)
			{
				if (ii.first.length() > src->Remaining())
					continue;

				//pick the longest matching operator/keyword
				if (ii.first.length() <= len)
					continue;

				//check if the characters match the operator/keyword
				if (src->IsOperator(ii.first))
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

				while (true)
				{
					char c = src->ConsumeChar();
					if (c == '*' && src->PeekChar() == '/')
					{
						src->ConsumeChar();
						break;
					}
					else if (c == 0)
					{
						diag->Error("Missing end to comment block starting at line " + std::to_string(startline), Token(t_ptr, 0, src->linenumber, column, TokenType::String, ""));
						throw 7;
					}
				}

				continue;
			}
			else if (toktype == TokenType::String)
			{
				std::string txt;

				//int start = index;
				while (src->IsAtEnd() == false)
				{
					char p = src->PeekChar();
					if (p == '\\')
					{
						src->ConsumeChar();
						//handle escape sequences
						char c = src->ConsumeChar();
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
							diag->Error("Invalid Escape Sequence '\\" + ch/*text.substr(index - 1, 1)*/ + "'", Token(t_ptr, 0, src->linenumber, src->column - 2, TokenType::String, std::string("\\") + ch));
							throw 7;
						}
					}
					else if (p == '"')
					{
						src->ConsumeChar();
						break;
					}
					else
					{
						txt.push_back(src->ConsumeChar());
					}
				}
				this->last_index = src->GetIndex();

				return Token(t_ptr, trivia_length, src->linenumber, column, toktype, txt);
			}
			else if (toktype == TokenType::BlockString)
			{
				diag->Error("Block Strings Not Implemented", Token(t_ptr, 0, src->linenumber, column, TokenType::String, ""));
				throw 7;
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
			else if (toktype == TokenType::IfMacro)
			{
				src->ConsumeChar();

				std::string condition;
				while (true)
				{
					char c = src->PeekChar();
					if (IsLetter(c) || IsNumber(c) || c == '_')
					{
						condition += c;
						src->ConsumeChar();
					}
					else
						break;
				}

				ifstack.push_back(condition);

				bool is_true = true;// this->defines[condition];
				if (is_true)
					continue;

				//read until we find and else or end if
				while (true)
				{
					char c = src->PeekChar();
					if (c != '#')
						src->ConsumeChar();
					else
					{
						//see if its an else or end if
						break;
					}
				}

				continue;
			}
			else if (toktype == TokenType::ElseMacro)
			{
				if (ifstack.size() == 0)
				{
					diag->Error("#else without matching #if", Token());
					throw 7;
				}

				continue;//just parse it for now
			}
			else if (toktype == TokenType::EndIfMacro)
			{
				if (ifstack.size() > 0)
					ifstack.pop_back();
				else
				{
					diag->Error("#endif without matching #if", Token());
					throw 7;
				}
				continue;
			}
			else if (toktype == TokenType::LocationMacro)
			{
				//parse to the end of the line and throw out for now, eventually going to use this to set
				//the line number
				std::string location;
				char c = src->EatChar();
				while (c != '\n' && c != 0)
				{
					location += c;
					c = src->EatChar();
				}

				//extract the line number from the end, start by finding last of @
				int index = location.find_last_of('@');
				std::string line = location.substr(index+1);
				int lnum = std::atoi(line.c_str());

				src->SetCurrentLine(lnum);

				if (c == 0)
					break;
				
				continue;
			}
			this->last_index = src->GetIndex();

			return Token(t_ptr, trivia_length, src->linenumber, column, toktype, str);
		}
		else if (IsLetter(c) || c == '_')//word
		{
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

			//check if it is a keyword
			auto keyword = ls.keywords.find(name);
			if (keyword != ls.keywords.end())//is keyword?
				return Token(t_ptr, trivia_length, src->linenumber, column, keyword->second, name);
			else//just a variable name
				return Token(t_ptr, trivia_length, src->linenumber, column, TokenType::Name, name);
		}
		else if (IsNumber(c))//number
		{
			char next_char = src->PeekChar();
			// Check if it is a standard integer or floating point number
			if (c != '0' || next_char == '.')
			{
				std::string num;
				num += c;
				while (true)
				{
					char c = src->PeekChar();
					if (c == 'f')
					{
						num += src->ConsumeChar();
						break;
					}
					if (!(c == '.' || IsNumber(c)))
						break;

					num += src->ConsumeChar();
				}

				this->last_index = src->GetIndex();

				return Token(t_ptr, trivia_length, src->linenumber, column, TokenType::Number, num);
			}
			else
			{
				//ok, its possibly some kind of non base ten literal
				std::string num;
				num += c;
				char c = next_char;
				num += c;
				switch (c)
				{
				case 'b':
				{
					src->ConsumeChar();
					while (true)
					{
						char c = src->PeekChar();
						if (!(c == '.' || c == '0' || c == '1'))// IsNumber(c)))
							break;

						num += src->ConsumeChar();
					}
					this->last_index = src->GetIndex();

					return Token(t_ptr, trivia_length, src->linenumber, column, TokenType::Number, num);
				}
				case 'x'://hex literal
				{
					src->ConsumeChar();
					while (true)
					{
						char c = src->PeekChar();
						if (!(c == '.' || IsNumber(c) || (c <= 'f' && c >= 'a') || (c <= 'F' && c >= 'A')))
							break;

						num += src->ConsumeChar();
					}
					this->last_index = src->GetIndex();

					return Token(t_ptr, trivia_length, src->linenumber, column, TokenType::Number, num);
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
					diag->Error("Invalid Escape Sequence '\\" + ch/*text.substr(index - 1, 1)*/ + "'", Token(t_ptr, 0, src->linenumber, src->column - 2, TokenType::String, std::string("\\") + ch));
					throw 7;
				}
				}
			}
			std::string num = std::to_string((int)cc);

			char endc = src->ConsumeChar();
			if (endc != '\'')
			{
				diag->Error("Closing ' expected for character literal.", Token(t_ptr, 0, src->linenumber, column, TokenType::String, ""));
				throw 7;
			}

			this->last_index = src->GetIndex();

			return Token(t_ptr, trivia_length, src->linenumber, column, TokenType::Number, num);
		}
		else
		{
			//character to ignore like whitespace
			if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
				continue;
			else
			{
				diag->Error("Unexpected character: '" + str + "'", Token(t_ptr, 0, src->linenumber, column, TokenType::String, ""));
				throw 7;
			}
		}
	}

	int trivia_length = src->GetIndex() - this->last_index;
	this->last_index = src->GetIndex();

	if (this->ifstack.size() > 0)
	{
		diag->Error("#if without matching #endif", Token());
		throw 7;
	}

	return Token(src->GetLinePointer(1) + src->GetLength(), trivia_length, src->linenumber, src->column, TokenType::EoF, "");
}

