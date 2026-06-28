#include <string.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <vadefs.h>


#include "tokenizer.h"
#include "memory.h"


#define ENABLE_PRINT_TOKENS


#ifdef ENABLE_PRINT_TOKENS
    #include "token_printer.h"
#endif


typedef struct Tokenizer {
    const char *source;
    const char *start;
    const char *current;
    Tokens *tokens;
    int line;
    bool hadErrors;
} Tokenizer;


static Tokenizer tokenizer;


static void initTokenizer() {
    tokenizer.source = NULL;
    tokenizer.start = NULL;
    tokenizer.current = NULL;
    tokenizer.tokens = NULL;
    tokenizer.line = 0;
    tokenizer.hadErrors = false;
}


static const char *source() { return tokenizer.source; }
static const char *start() { return tokenizer.start; }
static const char *current() { return tokenizer.current; }
static Tokens *tokens() { return tokenizer.tokens; }
static int line() { return tokenizer.line; }
static bool hadErrors() { return tokenizer.hadErrors; }


static void reportError(const char *format, ...) {
    tokenizer.hadErrors = true;

    va_list args;
    va_start(args, format);
    fprintf(stderr, "compile error: [at line %d], ", line());
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
}


static bool isAtEnd() {
    return *current() == '\0';
}


static char forward() {
    if (isAtEnd()) return '\0';
    return *tokenizer.current++;
}


static char peek() {
    return *current();
}


static char peekNext() {
    if (isAtEnd()) return '\0';
    return *(current() + 1);
}


static char match(char ch) {
    if (peek() == ch) return forward();
    return '\0';
}


static Token makeToken(TokenType type) {
    Token token;
    token.type = type;
    token.start = start();
    token.length = current() - start();
    token.line = line();
    return token;
}


static Token makeNewlineToken() {
    Token token = makeToken(TOKEN_NEWLINE);
    token.start = "";
    token.length = 0;
    return token;
}


static void appendToken(Token token) {
    if (tokens()->count >= tokens()->capacity) {
        size_t capacity = GROW_CAPACITY(tokens()->capacity);
        tokens()->tokens = GROW_ARRAY(Token, tokens()->tokens, capacity);
        tokens()->capacity = capacity;
    }

    tokens()->tokens[tokens()->count] = token;
    tokens()->count++;
}


static void skipWs() {
    for (;;) {
        switch (peek()) {
            // case '\n':
            //     appendToken(makeNewlineToken());
            //     tokenizer.line++;
            case ' ':
            case '\b':
            case '\r':
            case '\t':
                forward();
                break;
            default:
                return;
        }
    }
}


static void skipComments() {
#define FORWARD(n)  \
    do {                                            \
        for (int i = 0; i < n; i++) { forward(); }  \
    } while (false)

    if (peek() == '/' && peekNext() == '/') {
        FORWARD(2);
        for (;!isAtEnd() && peek() != '\n';) {
            forward();
        }
        if (peek() == '\n') {
            tokenizer.line++;
            forward();
        }
    }

    if (peek() == '/' && peekNext() == '*') {
        FORWARD(2);
        for (;!isAtEnd();) {
            if (peek() == '\n') tokenizer.line++;
            if (peek() == '*' && peekNext() == '/') {
                FORWARD(2);
                break;
            }
            forward();
        }
    }

#undef FORWARD
}


static bool isDigit(char ch) {
    return '0' <= ch && ch <= '9';
}


static bool isAlpha(char ch) {
    return ('a' <= ch && ch <= 'z') || ('A' <= ch && ch <= 'Z') || ch == '_';
}


static void number() {
    bool dotEncountered = false;
    for (;;) {
        if (isDigit(peek()) || (peek() == '.' && !dotEncountered)) {
            if (peek() == '.') dotEncountered = true;
            forward();
            continue;
        }
        break;
    }
    appendToken(makeToken(TOKEN_NUMBER));
}


static void string() {
    int errorLine = line();
    for (;!isAtEnd() && peek() != '"';) {
        if (peek() == '\n') tokenizer.line++;
        forward();
    }

    if (isAtEnd()) {
        int currentLine = line();
        tokenizer.line = errorLine;

        reportError("terminating '\"' is missing for string");

        tokenizer.line = currentLine;
    }

    tokenizer.start++;

    appendToken(makeToken(TOKEN_STRING));

    forward();
}


static TokenType checkKeyword(size_t startIndex, size_t length, const char *rest, TokenType type) {
    if (
        startIndex + length == (size_t)(current() - start()) &&
        memcmp(start() + startIndex, rest, length) == 0
    ) {
        return type;
    }

    return TOKEN_IDENTIFIER;
}


static TokenType tokenType() {
    switch (*start()) {
        case 'a': return checkKeyword(1, 2, "nd", TOKEN_AND);
        case 'b': return checkKeyword(1, 4, "reak", TOKEN_BREAK);
        case 'c': return checkKeyword(1, 7, "ontinue", TOKEN_CONTINUE);
        case 'e': {
            if (*(start() + 1) == 'x') return checkKeyword(2, 5, "tends", TOKEN_EXTENDS);
            TokenType type = checkKeyword(1, 5, "lseif", TOKEN_ELSE_IF);
            if (type == TOKEN_IDENTIFIER)
                type = checkKeyword(1, 3, "lse", TOKEN_ELSE);
            return type;
        }
        case 'f': {
            if (*(start() + 1) == 'o') return checkKeyword(2, 1, "r", TOKEN_FOR);
            if (*(start() + 1) == 'u') return checkKeyword(2, 2, "nc", TOKEN_FUNC);
            return checkKeyword(2, 3, "lse", TOKEN_FALSE);
        }
        case 'i': {
            TokenType type = checkKeyword(1, 1, "f", TOKEN_IF);
            if (type == TOKEN_IDENTIFIER)
                type = checkKeyword(1, 3, "nit", TOKEN_INIT);
            return type;
        }
        case 'n': return checkKeyword(1, 2, "il", TOKEN_NIL);
        case 'o': return checkKeyword(1, 1, "r", TOKEN_OR);
        case 'p': {
            TokenType type = checkKeyword(1, 4, "rint", TOKEN_PRINT);
            if (type == TOKEN_IDENTIFIER)
                type = checkKeyword(1, 6, "rintln", TOKEN_PRINTLN);
            return type;
        }
        case 'r': return checkKeyword(1, 5, "eturn", TOKEN_RETURN);
        case 's': {
            if (*(start() + 1) == 't') return checkKeyword(2, 4, "ruct", TOKEN_STRUCT);
            return checkKeyword(1, 4, "uper", TOKEN_SUPER);
        }
        case 't': {
            if (*(start() + 1) == 'r') return checkKeyword(2, 2, "ue", TOKEN_TRUE);
            return checkKeyword(1, 2, "his", TOKEN_THIS);
        }
        case 'v': return checkKeyword(1, 2, "ar", TOKEN_VAR);
    }
    return TOKEN_IDENTIFIER;
}


static void identifier() {
    for (;!isAtEnd() && (isAlpha(peek()) || isDigit(peek()));) {
        forward();
    }
    appendToken(makeToken(tokenType()));
}


static void scanToken() {
    skipWs();
    skipComments();

    tokenizer.start = current();

    char ch = forward();

    if (isDigit(ch)) return number();
    if (ch == '"') return string();
    if (isAlpha(ch)) return identifier();

    switch (ch) {
        case '+': {
            if (match('=')) {
                appendToken(makeToken(TOKEN_PLUS_EQUAL));
            } else {
                appendToken(makeToken(TOKEN_PLUS));
            }
            break;
        }
        case '-': {
            if (match('=')) {
                appendToken(makeToken(TOKEN_MINUS_EQUAL));
            } else {
                appendToken(makeToken(TOKEN_MINUS));
            }
            break;
        }
        case '*': {
            if (peek() == '*' && peekNext() == '=') {
                forward(); forward();
                appendToken(makeToken(TOKEN_STAR_STAR_EQUAL));
            } else if (peek() == '*') {
                forward();
                appendToken(makeToken(TOKEN_STAR_STAR));
            } else if (peek() == '=') {
                forward();
                appendToken(makeToken(TOKEN_STAR_EQUAL));
            } else {
                appendToken(makeToken(TOKEN_STAR));
            }
            break;
        }
        case '/': {
            if (match('=')) {
                appendToken(makeToken(TOKEN_SLASH_EQUAL));
            } else {
                appendToken(makeToken(TOKEN_SLASH));
            }
            break;
        }
        case '%': {
            if (match('=')) {
                appendToken(makeToken(TOKEN_PERCENT_EQUAL));
            } else {
                appendToken(makeToken(TOKEN_PERCENT));
            }
            break;
        }
        case '!': {
            if (match('=')) {
                appendToken(makeToken(TOKEN_BANG_EQUAL));
            } else {
                appendToken(makeToken(TOKEN_BANG));
            }
            break;
        }
        case '=': {
            if (match('=')) {
                appendToken(makeToken(TOKEN_EQUAL_EQUAL));
            } else {
                appendToken(makeToken(TOKEN_EQUAL));
            }
            break;
        }
        case '<': {
            if (match('=')) {
                appendToken(makeToken(TOKEN_LESSER_EQUAL));
            } else {
                appendToken(makeToken(TOKEN_LESSER));
            }
            break;
        }
        case '>': {
            if (match('=')) {
                appendToken(makeToken(TOKEN_GREATER_EQUAL));
            } else {
                appendToken(makeToken(TOKEN_GREATER));
            }
            break;
        }
        case '{': {
            appendToken(makeToken(TOKEN_LEFT_BRACE));
            break;
        }
        case '}': {
            appendToken(makeToken(TOKEN_RIGHT_BRACE));
            break;
        }
        case '(': {
            appendToken(makeToken(TOKEN_LEFT_PAREN));
            break;
        }
        case ')': {
            appendToken(makeToken(TOKEN_RIGHT_PAREN));
            break;
        }
        case ',': {
            appendToken(makeToken(TOKEN_COMMA));
            break;
        }
        case ';': {
            appendToken(makeToken(TOKEN_SEMICOLON));
            break;
        }
        case ':': {
            appendToken(makeToken(TOKEN_COLON));
            break;
        }
        case '?': {
            appendToken(makeToken(TOKEN_QUESTION));
            break;
        }
        case '.': {
            appendToken(makeToken(TOKEN_DOT));
            break;
        }
        case '\n': {
            appendToken(makeNewlineToken());
            tokenizer.line++;
            break;
        }
        default: {
            reportError("unknown character '%c' encountered", ch);
        }
    }
}


void initTokens(Tokens *tokens) {
    tokens->capacity = 0;
    tokens->count = 0;
    tokens->tokens = NULL;
}


void freeTokens(Tokens *tokens) {
    FREE(tokens->tokens);
    initTokens(tokens);
}


bool getTokens(const char *source, Tokens *tokens) {
    initTokenizer();

    tokenizer.source = source;
    tokenizer.start = tokenizer.source;
    tokenizer.current = tokenizer.source;
    tokenizer.tokens = tokens;
    tokenizer.line = 1;

    for (;!isAtEnd();) {
        scanToken();
    }
    tokenizer.start = current();
    appendToken(makeToken(TOKEN_EOF));

#ifdef ENABLE_PRINT_TOKENS
    printTokens(tokens);
#endif

    return !hadErrors();
}
