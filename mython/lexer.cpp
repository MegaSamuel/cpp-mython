#include "lexer.h"

#include <algorithm>
#include <charconv>
#include <unordered_map>

using namespace std;

namespace parse {

bool operator==(const Token& lhs, const Token& rhs) {
    using namespace token_type;

    if (lhs.index() != rhs.index()) {
        return false;
    }
    if (lhs.Is<Char>()) {
        return lhs.As<Char>().value == rhs.As<Char>().value;
    }
    if (lhs.Is<Number>()) {
        return lhs.As<Number>().value == rhs.As<Number>().value;
    }
    if (lhs.Is<String>()) {
        return lhs.As<String>().value == rhs.As<String>().value;
    }
    if (lhs.Is<Id>()) {
        return lhs.As<Id>().value == rhs.As<Id>().value;
    }
    return true;
}

bool operator!=(const Token& lhs, const Token& rhs) {
    return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os, const Token& rhs) {
    using namespace token_type;

#define VALUED_OUTPUT(type) \
    if (auto p = rhs.TryAs<type>()) return os << #type << '{' << p->value << '}';

    VALUED_OUTPUT(Number);
    VALUED_OUTPUT(Id);
    VALUED_OUTPUT(String);
    VALUED_OUTPUT(Char);

#undef VALUED_OUTPUT

#define UNVALUED_OUTPUT(type) \
    if (rhs.Is<type>()) return os << #type;

    UNVALUED_OUTPUT(Class);
    UNVALUED_OUTPUT(Return);
    UNVALUED_OUTPUT(If);
    UNVALUED_OUTPUT(Else);
    UNVALUED_OUTPUT(Def);
    UNVALUED_OUTPUT(Newline);
    UNVALUED_OUTPUT(Print);
    UNVALUED_OUTPUT(Indent);
    UNVALUED_OUTPUT(Dedent);
    UNVALUED_OUTPUT(And);
    UNVALUED_OUTPUT(Or);
    UNVALUED_OUTPUT(Not);
    UNVALUED_OUTPUT(Eq);
    UNVALUED_OUTPUT(NotEq);
    UNVALUED_OUTPUT(LessOrEq);
    UNVALUED_OUTPUT(GreaterOrEq);
    UNVALUED_OUTPUT(None);
    UNVALUED_OUTPUT(True);
    UNVALUED_OUTPUT(False);
    UNVALUED_OUTPUT(Eof);

#undef UNVALUED_OUTPUT

    return os << "Unknown token :("sv;
}

Lexer::Lexer(istream& input) : token_input_(input) {
    current_token_ = vct_tokens_.begin();
    dent_ = 0;

    ParseTokens();
}

const Token& Lexer::CurrentToken() const {
        return *current_token_;
}

Token Lexer::NextToken() {
    if (vct_tokens_.end() == (current_token_+1)) {
        return *current_token_;
    }
    return *(++current_token_);
}

void Lexer::ParseTokens() {
    TrimSpaces(token_input_);

    while (token_input_) {
        ParseString(token_input_);
        ParseKeywords(token_input_);
        ParseChars(token_input_);
        ParseNumbers(token_input_);
        TrimSpaces(token_input_);
        ParseNewLine(token_input_);
    }

    if (!vct_tokens_.empty() && (!vct_tokens_.back().Is<token_type::Newline>())) {
        vct_tokens_.emplace_back(token_type::Newline{});
    }

    while (dent_ > 0) {
        vct_tokens_.emplace_back(token_type::Dedent{});
        --dent_;
    }

    vct_tokens_.emplace_back(token_type::Eof{});

    current_token_ = vct_tokens_.begin();
}

void Lexer::TrimSpaces(istream& in) {
    while (in.peek() == ' ') {
        in.get();
    }
}

bool Lexer::CheckEof(istream& in) {
    return (in.peek() == char_traits<char>::eof());
}

void Lexer::ParseDent(istream& in) {
    if (CheckEof(in)) {
        return;
    }
    char ch;
    int spaces_processed = 0;
    while (in.get(ch) && (ch == ' ')) {
        ++spaces_processed;
    }
    if (in.rdstate() != ios_base::eofbit) {
        in.putback(ch);
        if (ch == '\n') {
            return;
        }
    }
    if (dent_*DENT_SPACES < spaces_processed) {
        spaces_processed -= dent_*DENT_SPACES;
        while (spaces_processed > 0) {
            spaces_processed -= DENT_SPACES;
            vct_tokens_.emplace_back(token_type::Indent{});
            ++dent_;
        }
    }
    else if (dent_*DENT_SPACES > spaces_processed)
    {
        spaces_processed = dent_*DENT_SPACES - spaces_processed;
        while (spaces_processed > 0) {
            spaces_processed -= DENT_SPACES;
            vct_tokens_.emplace_back(token_type::Dedent{});
            --dent_;
        }
    }
}

void Lexer::ParseString(istream& in) {
    if (CheckEof(in)) {
        return;
    }
    char begin = in.get();
    if ((begin == '\'') || (begin == '\"'))
    {
        char ch;
        string result;
        while (in.get(ch)) {
            if (ch == begin) {
                break;
            }
            else if (ch == '\\') {
                char esc_ch;
                if (in.get(esc_ch))
                {
                    switch (esc_ch)
                    {
                    case 'n':
                        result.push_back('\n');
                        break;
                    case 't':
                        result.push_back('\t');
                        break;
                    case 'r':
                        result.push_back('\r');
                        break;
                    case '"':
                        result.push_back('"');
                        break;
                    case '\'':
                        result.push_back('\'');
                        break;
                    case '\\':
                        result.push_back('\\');
                        break;
                    default:
                        throw LexerError(string(__func__) + " is failed at " + to_string(__LINE__));
                    }
                }
                else {
                    throw LexerError(string(__func__) + " is failed at " + to_string(__LINE__));
                }
            }
            else if ((ch == '\n') || (ch == '\r')) {
                throw LexerError(string(__func__) + " is failed at " + to_string(__LINE__));
            }
            else {
                result.push_back(ch);
            }
        }
        if (begin == ch) {
            vct_tokens_.emplace_back(token_type::String{ result });
        }
        else {
            throw LexerError(string(__func__) + " is failed at " + to_string(__LINE__));
        }
    }
    else {
        in.putback(begin);
    }
}

void Lexer::ParseKeywords(istream& in) {
    if (CheckEof(in)) {
        return;
    }
    char ch = in.peek();
    if (isalpha(ch) || ch == '_')
    {
        string keyword;
        while (in.get(ch))
        {
            if (isalnum(ch) || ch == '_') {
                keyword.push_back(ch);
            }
            else  {
                in.putback(ch);
                break;
            }
        }
        if (!keyword.compare("class")) {
            vct_tokens_.emplace_back(token_type::Class{});
        }
        else if (!keyword.compare("return")) {
            vct_tokens_.emplace_back(token_type::Return{});
        }
        else if (!keyword.compare("if")) {
            vct_tokens_.emplace_back(token_type::If{});
        }
        else if (!keyword.compare("else")) {
            vct_tokens_.emplace_back(token_type::Else{});
        }
        else if (!keyword.compare("def")) {
            vct_tokens_.emplace_back(token_type::Def{});
        }
        else if (!keyword.compare("print")) {
            vct_tokens_.emplace_back(token_type::Print{});
        }
        else if (!keyword.compare("and")) {
            vct_tokens_.emplace_back(token_type::And{});
        }
        else if (!keyword.compare("or")) {
            vct_tokens_.emplace_back(token_type::Or{});
        }
        else if (!keyword.compare("not")) {
            vct_tokens_.emplace_back(token_type::Not{});
        }
        else if (!keyword.compare("None")) {
            vct_tokens_.emplace_back(token_type::None{});
        }
        else if (!keyword.compare("True")) {
            vct_tokens_.emplace_back(token_type::True{});
        }
        else if (!keyword.compare("False")) {
            vct_tokens_.emplace_back(token_type::False{});
        }
        else {
            vct_tokens_.emplace_back(token_type::Id{keyword});
        }
    }
}

void Lexer::ParseChars(istream& in) {
    if (CheckEof(in)) {
        return;
    }
    char ch;
    in.get(ch);
    if (ispunct(ch)) {
        if (ch == '#') {
            in.putback(ch);
            ParseComments(in);
            return;
        }
        else if ((ch == '=') && (in.peek() == '=')) {
            in.get();
            vct_tokens_.emplace_back(token_type::Eq{});
        }
        else if ((ch == '!') && (in.peek() == '=')) {
            in.get();
            vct_tokens_.emplace_back(token_type::NotEq{});
        }
        else if ((ch == '>') && (in.peek() == '=')) {
            in.get();
            vct_tokens_.emplace_back(token_type::GreaterOrEq{});
        }
        else if ((ch == '<') && (in.peek() == '=')) {
            in.get();
            vct_tokens_.emplace_back(token_type::LessOrEq{});
        }
        else {
            vct_tokens_.emplace_back(token_type::Char{ ch });
        }
    }
    else {
        in.putback(ch);
    }
}

void Lexer::ParseNumbers(istream& in) {
    if (CheckEof(in)) {
        return;
    }
    char ch = in.peek();
    if (isdigit(ch)) {
        string result;
        while (in.get(ch)) {
            if (isdigit(ch)) {
                result.push_back(ch);
            }
            else {
                in.putback(ch);
                break;
            }
        }
        int num = stoi(result);
        vct_tokens_.emplace_back(token_type::Number{ num });
    }
}

void Lexer::ParseNewLine(istream& in) {
    char ch = in.peek();
    if (ch == '\n') {
        in.get(ch);
        if (!vct_tokens_.empty() && (!vct_tokens_.back().Is<token_type::Newline>())) {
            vct_tokens_.emplace_back(token_type::Newline{});
        }
        ParseDent(in);
    }
}

void Lexer::ParseComments(istream& in) {
    char ch = in.peek();
    if (ch == '#') {
        string tmp_str;
        getline(in, tmp_str, '\n');
        if (in.rdstate() != ios_base::eofbit) {
            in.putback('\n');
        }
    }
}

}  // namespace parse