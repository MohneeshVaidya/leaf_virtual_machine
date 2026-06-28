#ifndef leaf_tokenizer_h
#define leaf_tokenizer_h


#include <stdbool.h>
#include <stddef.h>


typedef enum TokenType {
    TOKEN_PRINT,
    TOKEN_PRINTLN,
    TOKEN_VAR,
    TOKEN_IF,
    TOKEN_ELSE_IF,
    TOKEN_ELSE,
    TOKEN_FOR,
    TOKEN_BREAK,
    TOKEN_CONTINUE,
    TOKEN_FUNC,
    TOKEN_RETURN,
    TOKEN_STRUCT,
    TOKEN_INIT,
    TOKEN_THIS,
    TOKEN_SUPER,
    TOKEN_EXTENDS,
    TOKEN_NEWLINE,

    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_NIL,

    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_PERCENT,
    TOKEN_STAR_STAR,
    TOKEN_AND,
    TOKEN_OR,
    TOKEN_BANG,

    TOKEN_EQUAL,
    TOKEN_PLUS_EQUAL,
    TOKEN_MINUS_EQUAL,
    TOKEN_STAR_EQUAL,
    TOKEN_SLASH_EQUAL,
    TOKEN_PERCENT_EQUAL,
    TOKEN_STAR_STAR_EQUAL,

    TOKEN_EQUAL_EQUAL,
    TOKEN_BANG_EQUAL,
    TOKEN_LESSER,
    TOKEN_LESSER_EQUAL,
    TOKEN_GREATER,
    TOKEN_GREATER_EQUAL,

    TOKEN_LEFT_BRACE,
    TOKEN_RIGHT_BRACE,
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_COMMA,
    TOKEN_SEMICOLON,
    TOKEN_COLON,
    TOKEN_QUESTION,
    TOKEN_DOT,

    TOKEN_ERROR,
    TOKEN_EOF,

} TokenType;


typedef struct Token {
    TokenType type;
    const char *start;
    int length;
    int line;
} Token;



typedef struct Tokens {
    size_t capacity;
    size_t count;
    Token *tokens;
} Tokens;


void initTokens(Tokens *tokens);
void freeTokens(Tokens *tokens);
bool getTokens(const char *source, Tokens *tokens);


#endif
