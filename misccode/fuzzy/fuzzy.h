#ifndef FUZZY_WEIGHER__
#define FUZZY_WEIGHER__

/* C headers */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>

/* C++ headers */
#include <iostream>
#include <queue>
#include <vector>
#include <string>

class Token {
public:
    enum { 
        TOK_NULL = 0,
        TOK_NUMBER = 1, 
        TOK_IDENT = 2, 
        TOK_NONSPACE = 3, 
        TOK_LAST = TOK_NONSPACE,
    };

public:
    Token(int type) : ttype_(type) {}
    Token(int type, double num) : ttype_(type), numvalue_(num) {}
    Token(int type, const std::string &str) : ttype_(type), strvalue_(str) {}
    Token(int type, double num, 
            const std::string &str) 
        : ttype_(type), numvalue_(num), strvalue_(str) {}
    virtual ~Token() {}

    int type() const { return ttype_; }
    std::string str() const { return strvalue_; }
    double num() const { return numvalue_; }

    virtual bool is_accessor() const { return false; }

private:
    int ttype_;
    std::string strvalue_;
    double numvalue_;
};

template <typename T>
class AccessorToken: public Token {
public:
    typedef double (T::*field_accessor_type)(void) const;

public:
    AccessorToken(): Token(Token::TOK_NULL)
    {}

    AccessorToken(const std::string &name, field_accessor_type func)
        : Token(Token::TOK_NULL, name), func_(func)
    {}

    field_accessor_type accessor() const {
        return func_;
    }

    double apply(const T &obj) const {
        return (obj.*func_)();
    }

    bool is_accessor() const { return true; }

private:
    field_accessor_type func_;
};

class Lexer {
public:
    Lexer() 
    {}

    void start_lex(const std::string &str) {
        p_ = str.data();
        e_ = str.data() + str.length();
    }

    const Token *lex() {
        while (p_ < e_) {
            skip_whitespace();
            if (isdigit(*p_)) {
                return new Token(Token::TOK_NUMBER, lex_number());
            } else if (isalpha(*p_)) {
                return new Token(Token::TOK_IDENT, lex_identifier());
            } else {
                return new Token(Token::TOK_NONSPACE, lex_nonspace());
            }
        }
        return NULL;
    }

private:
    void skip_whitespace() {
        while (p_ < e_ && isspace(*p_)) {
            ++p_;
        }
    }

    std::string lex_identifier() {
        std::string result;
        while (p_ < e_ && isalpha(*p_)) {
            result.push_back(*p_);
            ++p_;
        }
        return result;
    }

    std::string lex_nonspace() {
        std::string result;
        while (p_ < e_ && !isspace(*p_)) {
            result.push_back(*p_);
            ++p_;
            break;
        }
        return result;
    }

    double lex_number() {
        int result = 0;
        while (p_ < e_ && isdigit(*p_)) {
            result = result * 10 + (*p_ - '0');
            ++p_;
        }
        return result;
    }

private:
    const char *p_, *e_;
};

template <typename T>
class Node {
public:
    virtual ~Node() {}
    virtual double calc(T *p) = 0;
};

template <typename T>
class Return: public Node<T> {
public:
    Return(double v)
        : value_(v) 
    {}

    virtual double calc(T *p) {
        return value_;
    }

private:
    double value_;
};

template <typename T>
class Switch: public Node<T> {
public:
    Switch()
    {}

    void set_field(const AccessorToken<T> &field) {
        field_ = field; 
    }

    void insert_case(double value, Node<T> *node) {
        cases_.push_back(std::make_pair(value, node));
    }

    void set_default(Node<T> *node) {
        default_ = node;
    }

    virtual double calc(T *p) {
        double val = field_.apply(*p);

        const NPair &first = cases_.front();
        const NPair &last = cases_.back();

        if (val < first.first || val > last.first || cases_.size() == 1) {
            return default_->calc(p);
        }

        for (int i = 1; i < cases_.size(); ++i) {
            if (cases_[i].first > val) {
                double dist = cases_[i].first - cases_[i - 1].first;
                return 
                    (val - cases_[i - 1].first) / dist
                        * cases_[i - 1].second->calc(p)
                    + (cases_[i].first - val) / dist
                        * cases_[i].second->calc(p);
            }
        }

        /* can't be here */
        assert(0);
    }

private:
    AccessorToken<T> field_;
    typedef std::pair<double, Node<T> *> NPair;
    std::vector<NPair> cases_;
    Node<T> *default_;
};

template <typename T>
class FuzzyWeigher {
public:
    FuzzyWeigher(Node<T> *root)
        : root_(root)
    {}

    double operator()(T *p) const {
        return root_->calc(p);
    }

private:
    Node<T> *root_;
};

template <typename T>
FuzzyWeigher<T>
construct_fuzzy_weigher(Node<T> *root) {
    return FuzzyWeigher<T>(root);
}

template <typename T>
class TokenLexer {
public:
    TokenLexer() {
    }

    void start_lex(const std::string &str) {
        lexer_.start_lex(str);
        while (!queue_.empty()) { queue_.pop(); }
    }

    void register_builtin_token(const std::string &name, int id) {
        builtins_.push_back(new Token(id, name));
    }

    void register_accessor_token(const std::string &name,
        typename AccessorToken<T>::field_accessor_type accessor) 
    {
        accessor_tokens_.push_back(
            new AccessorToken<T>(std::string(name), accessor));
    }

    const Token *lex() {
        if (!queue_.empty()) {
            const Token *top = queue_.back();
            queue_.pop();
            return top;
        }

        const Token *token = lexer_.lex();
        if (token->type() == Token::TOK_IDENT) {
            for (int i = 0; i < builtins_.size(); ++i) {
                if (token->str() == builtins_[i]->str()) {
                    return builtins_[i];
                }
            }
            for (int i = 0; i < accessor_tokens_.size(); ++i) {
                if (token->str() == accessor_tokens_[i]->str()) {
                    return accessor_tokens_[i];
                }
            }
        }
        return token;
    }

    void unlex(const Token *token) {
        queue_.push(token);
    }

private:
    Lexer lexer_;
    std::queue<const Token*> queue_;
    std::vector<Token*> builtins_;
    std::vector<Token*> accessor_tokens_;
};

/* parser with accessors */
template <typename T>
class NodeParser {
private:
    enum {
        TOK_SWITCH = Token::TOK_LAST + 1,
        TOK_CASE,
        TOK_DEFAULT,
        TOK_RETURN,
    };

    enum switch_states {
        EXPECTS_SWITCH,
        SWITCH_GET_FIELD,
        EXPECTS_BRACE,
        EXPECTS_CASE,
    };

public:
    NodeParser() {
        /* builtins */
        token_lexer_.register_builtin_token("switch", TOK_SWITCH);
        token_lexer_.register_builtin_token("case", TOK_CASE);
        token_lexer_.register_builtin_token("default", TOK_DEFAULT);
        token_lexer_.register_builtin_token("return", TOK_RETURN);
    }

    void register_accessor(const std::string &name,
        typename AccessorToken<T>::field_accessor_type accessor) 
    { token_lexer_.register_accessor_token(name, accessor); }

    Node<T> *parse(const std::string &str) {
        Node<T> *node;
        token_lexer_.start_lex(str);
        return parse_switch();
    }

    Node<T> *parse_switch() {
        const Token *token;
        Switch<T> *result = new Switch<T>();
        int state = EXPECTS_SWITCH;
        while ((token = token_lexer_.lex())) {
            switch (state) {
            case EXPECTS_SWITCH: {
                if (token->type() != TOK_SWITCH)
                    return parse_error("Expecting switch token");
                state = SWITCH_GET_FIELD;
                break;
            }

            case SWITCH_GET_FIELD: {
                if (token->type() != Token::TOK_NONSPACE || token->str() != "(")
                    return parse_error("Expecting '(' after switch ");

                token = token_lexer_.lex();

                if (!token || !token->is_accessor())
                    return parse_error("Expecting access in switch(");

                result->set_field(*dynamic_cast<const AccessorToken<T>*>(token));

                token = token_lexer_.lex();

                if (!token || token->type() != Token::TOK_NONSPACE 
                        || token->str() != ")")
                    return parse_error("Expecting ')' after switch ");

                state = EXPECTS_BRACE;
                break;
            }

            case EXPECTS_BRACE: {
                if (token->type() != Token::TOK_NONSPACE || token->str() != "{")
                    return parse_error("Expecting '{' after switch (x)");
                state = EXPECTS_CASE;
                break;
            }

            case EXPECTS_CASE: {
                if (token->type() == Token::TOK_NONSPACE || token->str() == "}")
                    return result;

                if (token->type() == TOK_DEFAULT) {
                    token = token_lexer_.lex();
                    if (!token || token->type() != Token::TOK_NONSPACE 
                            || token->str() != ":")
                        return parse_error("Expecting ':' after default");

                    Node<T> *node = parse_case();
                    if (!node) {
                        return NULL;
                    }
                    result->set_default(node);
                    break;
                }

                token = token_lexer_.lex();

                if (!token || token->type() != Token::TOK_NUMBER)
                    return parse_error("Expecting number in case statement");

                double num = token->num();

                token = token_lexer_.lex();
                if (!token || token->type() != Token::TOK_NONSPACE 
                        || token->str() != ":")
                    return parse_error("Expecting ':' after default");

                Node<T> *node = parse_case();
                if (!node) {
                    return NULL;
                }
                result->insert_case(num, node);

                state = EXPECTS_CASE;
                break;
            }};
        }
    }

    Node<T> *parse_case() {
        const Token *token;

        token = token_lexer_.lex();
        if (token && token->type() == TOK_RETURN) {
            token = token_lexer_.lex();
            if (!token || token->type() != Token::TOK_NUMBER)
                return parse_error("Expecting number in return statement ");
            return new Return<T>(token->num());
        }
        if (token && token->type() == TOK_SWITCH) {
            token_lexer_.unlex(token);
            return parse_switch();
        }
        return parse_error("Expecting default or switch in case body");
    }

private:
    /* for convinience returns null */
    Node<T> *parse_error(const char *format, ...) {
        va_list ap;
        va_start(ap, format);
        vfprintf(stderr, format, ap);
        fprintf(stderr, "\n");
        return NULL;
    }

private:
    TokenLexer<T> token_lexer_;
};

#endif /* FUZZY_WEIGHER__ */
