#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "compiler.h"
#include "chunk.h"
#include "object.h"
#include "tokenizer.h"
#include "value.h"
#include "vm.h"


typedef struct Compiler {
    Token *tokens;
    Token *current;
    Token *previous;
    Chunk *chunk;
    bool hadErrors;
    int loopDepth;
} Compiler;


static Compiler compiler;


static void initCompiler(Token *tokens, Chunk *chunk) {
    compiler.tokens = tokens;
    compiler.current = compiler.tokens;
    compiler.previous = NULL;
    compiler.chunk = chunk;
    compiler.hadErrors = false;
    compiler.loopDepth = 0;
}


static void freeCompiler() {
    initCompiler(NULL, NULL);
}


static Token *tokens() { return compiler.tokens; }
static Token *current() { return compiler.current; }
static Token *previous() { return compiler.previous; }
static Chunk *chunk() { return compiler.chunk; }
static bool hadErrors() { return compiler.hadErrors; }
static bool loopDepth() { return compiler.loopDepth; }


static void errorAt(Token *token, const char *errorMessage) {
    compiler.hadErrors = true;
    fprintf(stderr, "compile error: [at line %d], %s\n", token->line, errorMessage);
}


static void errorAtCurrent(const char *errorMessage) {
    errorAt(current(), errorMessage);
}


static void errorAtPrevious(const char *errorMessage) {
    errorAt(previous(), errorMessage);
}


static bool isAtEnd() {
    return current()->type == TOKEN_EOF;
}


static Token *forward() {
    compiler.previous = current();

    if (isAtEnd()) return current();

    return compiler.current++;
}


static Token *backward() {
    if (current() == tokens()) {
        return NULL;
    } else if (current() - tokens() > 1) {
        compiler.previous = current() - 2;
    }
    return compiler.current--;
}


static Token *match(TokenType type) {
    if (current()->type == type) return forward();
    return NULL;
}


static Token *matchAny(TokenType *types, int length) {
    for (int i = 0; i < length; i++) {
        if (types[i] == current()->type) return forward();
    }
    return NULL;
}


static Token *consume(TokenType type, const char *errorMessage) {
    if (current()->type == type) {
        return forward();
    }

    errorAt(previous(), errorMessage);
    return NULL;
}


static void emitByte(Operation byte) {
    chunkAdd(chunk(), byte, previous() ? previous()->line : current()->line);
}


static void emitBytes(Operation byte1, Operation byte2) {
    emitByte(byte1);
    emitByte(byte2);
}


static int makeConstant(Value value) {
    return addConstant(chunk(), value);
}


static int addLocal(int index) {
    ObjString *name = AS_STRING(chunk()->constantPool[index]);

    Scope *scope = &vm.scope;

    for (int i = scope->localCount - 1; (i > -1) && (scope->locals[i].depth == scope->scopeDepth); i-- ) {
        if (name == scope->locals[i].name) {
            return -1;
        }
    }

    scope->locals[scope->localCount].name = name;
    scope->locals[scope->localCount].depth = scope->scopeDepth;
    return scope->localCount++;
}


static int resolveLocal(int index) {
    ObjString *name = AS_STRING(chunk()->constantPool[index]);

    Scope *scope = &vm.scope;
    for (int i = scope->localCount - 1; i > -1; i--) {
        if (name == scope->locals[i].name) {
            return i;
        }
    }
    return -1;
}


typedef enum Precedence {
    PRECEDENCE_NONE,
    PRECEDENCE_COMMA,
    PRECEDENCE_ASSIGN,
    PRECEDENCE_TERNARY,
    PRECEDENCE_OR,
    PRECEDENCE_AND,
    PRECEDENCE_EQUALITY,
    PRECEDENCE_COMPARISON,
    PRECEDENCE_TERM,
    PRECEDENCE_FACTOR,
    PRECEDENCE_POWER,
    PRECEDENCE_UNARY,
    PRECEDENCE_CALL,
    PRECEDENCE_TERMINAL,
} Precedence;


typedef void (*Function)();


typedef struct Rule {
    Function prefix;
    Function infix;
    Precedence precedence;
} Rule;


static Rule rules[];


static void parsePrecedence(Precedence precedence);
static void expression();
static void statement(size_t *breakJump, size_t *continueJump);
static size_t makeJump();
static void patchJump(size_t jump);


static Rule *getRule(TokenType type) {
    return &rules[type];
}


static bool assign(int index, Operation opGet, Operation opSet) {
    if (match(TOKEN_EQUAL)) {
        parsePrecedence(PRECEDENCE_ASSIGN);
        emitBytes(opSet, index);
        return true;
    }
    if (matchAny((TokenType[]){ TOKEN_PLUS_EQUAL, TOKEN_MINUS_EQUAL, TOKEN_STAR_EQUAL, TOKEN_SLASH_EQUAL, TOKEN_PERCENT_EQUAL, TOKEN_STAR_STAR_EQUAL }, 6)) {
        Token *token = previous();

        emitBytes(opGet, index);
        parsePrecedence(PRECEDENCE_ASSIGN);

        switch (token->type) {
            case TOKEN_PLUS_EQUAL: {
                emitByte(OP_ADD);
                break;
            }
            case TOKEN_MINUS_EQUAL: {
                emitByte(OP_SUB);
                break;
            }
            case TOKEN_STAR_EQUAL: {
                emitByte(OP_MUL);
                break;
            }
            case TOKEN_SLASH_EQUAL: {
                emitByte(OP_DIV);
                break;
            }
            case TOKEN_PERCENT_EQUAL: {
                emitByte(OP_MOD);
                break;
            }
            case TOKEN_STAR_STAR_EQUAL: {
                emitByte(OP_POW);
                break;
            }
            default:
                break;
        }

        emitBytes(opSet, index);
        return true;
    }
    return false;
}


static void identifier() {
    int index = makeConstant(OBJ_VALUE(makeString(previous()->start, previous()->length)));

    int localIndex = resolveLocal(index);

    Operation opGet;
    Operation opSet;

    if (localIndex == -1) {
        opGet = OP_GET_GLOBAL;
        opSet = OP_SET_GLOBAL;
    } else {
        opGet = OP_GET_LOCAL;
        opSet = OP_SET_LOCAL;
    }

    if (!assign(localIndex == -1 ? index : localIndex, opGet, opSet)) {
        emitBytes(opGet, localIndex == -1 ? index : localIndex);
    }
}


static void number() {
    Value value = NUMBER_VALUE(strtod(previous()->start, NULL));
    emitBytes(OP_CONSTANT, makeConstant(value));
}


static void string() {
    Value value = OBJ_VALUE(makeString(previous()->start, previous()->length));
    emitBytes(OP_CONSTANT, makeConstant(value));
}


static void boolean() {
    Value value = BOOLEAN_VALUE((memcmp(previous()->start, "true", previous()->length) == 0));
    emitBytes(OP_CONSTANT, makeConstant(value));
}


static void nil() {
    emitBytes(OP_CONSTANT, makeConstant(NIL_VALUE));
}


static void unary() {
    TokenType type = previous()->type;
    parsePrecedence(PRECEDENCE_UNARY);

    switch (type) {
        case TOKEN_MINUS: return emitByte(OP_NEGATE);
        case TOKEN_BANG: return emitByte(OP_NOT);
        case TOKEN_PLUS: return emitByte(OP_POS);
        default:
            return;
    }
}


static void binary() {
    TokenType type = previous()->type;
    parsePrecedence(getRule(type)->precedence + 1);

    switch (type) {
        case TOKEN_PLUS: return emitByte(OP_ADD);
        case TOKEN_MINUS: return emitByte(OP_SUB);
        case TOKEN_STAR: return emitByte(OP_MUL);
        case TOKEN_SLASH: return emitByte(OP_DIV);
        case TOKEN_PERCENT: return emitByte(OP_MOD);
        case TOKEN_EQUAL_EQUAL: return emitByte(OP_EQUAL);
        case TOKEN_BANG_EQUAL: return emitByte(OP_NOT_EQUAL);
        case TOKEN_LESSER: return emitByte(OP_LESSER);
        case TOKEN_LESSER_EQUAL: return emitByte(OP_LESSER_EQUAL);
        case TOKEN_GREATER: return emitByte(OP_GREATER);
        case TOKEN_GREATER_EQUAL: return emitByte(OP_GREATER_EQUAL);
        case TOKEN_COMMA: return emitByte(OP_COMMA);
        default:
            return;
    }
}


static void power() {
    parsePrecedence(PRECEDENCE_POWER);
    emitByte(OP_POW);
}


static void and_() {
    emitByte(OP_JUMP_IF_FALSE);
    size_t jumpIfFalse = makeJump();

    emitByte(OP_POP);
    parsePrecedence(PRECEDENCE_AND);

    emitByte(OP_NOP);

    patchJump(jumpIfFalse);
}


static void or_() {
    emitByte(OP_JUMP_IF_FALSE);
    size_t jumpIfFalse = makeJump();

    emitByte(OP_JUMP);
    size_t jumpEnd = makeJump();

    emitByte(OP_POP);
    patchJump(jumpIfFalse);

    parsePrecedence(PRECEDENCE_OR);

    emitByte(OP_NOP);
    patchJump(jumpEnd);
}


static void ternary() {
    emitByte(OP_JUMP_IF_FALSE);
    size_t jumpIfFalse = makeJump();

    emitByte(OP_POP);

    expression();

    emitByte(OP_JUMP);
    size_t jumpEnd = makeJump();

    consume(TOKEN_COLON, "expect ':' after first expression in ternary operator");

    emitByte(OP_POP);
    patchJump(jumpIfFalse);

    expression();

    emitByte(OP_NOP);
    patchJump(jumpEnd);
}


static void call() {

}


static void grouping() {
    expression();
    consume(TOKEN_RIGHT_PAREN, "expect ')' to terminate grouping expression");
}


static void parsePrecedence(Precedence precedence) {
    forward();

    Function prefixRule = getRule(previous()->type)->prefix;
    if (prefixRule == NULL) {
        errorAtPrevious("prefix expected");
        return;
    }

    prefixRule();

    for (;precedence <= getRule(current()->type)->precedence;) {
        forward();
        Function infixRule = getRule(previous()->type)->infix;
        infixRule();
    }
}


static void expression() {
    parsePrecedence(PRECEDENCE_COMMA);
}


static void expressionStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "expect ';' at the end of statement");
    emitByte(OP_POP);
}


static void printStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "expect ';' at the end of statement");
    emitByte(OP_PRINT);
}


static void printlnStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "expect ';' at the end of statement");
    emitByte(OP_PRINTLN);
}


static int parseIdentifier() {
    Token *token = consume(TOKEN_IDENTIFIER, "expect a name for declaration statement");
    return makeConstant(OBJ_VALUE(makeString(token->start, token->length)));
}


static void varStatement() {
    int index = parseIdentifier();


    if (match(TOKEN_EQUAL)) {
        expression();
    } else {
        emitBytes(OP_CONSTANT, makeConstant(NIL_VALUE));
    }

    consume(TOKEN_SEMICOLON, "expect ';' after var declaration");

    Operation operation;
    if (vm.scope.scopeDepth > 0) {
        (void)addLocal(index);
        operation = OP_DECLARE_LOCAL;
    } else {
        operation = OP_DECLARE_GLOBAL;
    }

    emitBytes(operation, index);
}


static void beginScope() {
    vm.scope.scopeDepth++;
}


static void endScope() {
    vm.scope.scopeDepth--;

    Scope *scope = &vm.scope;
    int localCount = scope->localCount;
    for (;localCount > 0 && scope->locals[localCount-1].depth > scope->scopeDepth; localCount--) {
        emitByte(OP_POP);
    }
    scope->localCount = localCount;
}


static void blockStatement(size_t *breakJump, size_t *continueJump) {
    for (;!isAtEnd() && !match(TOKEN_RIGHT_BRACE);) {
        statement(breakJump, continueJump);
    }
    if (previous()->type != TOKEN_RIGHT_BRACE) {
        errorAt(current(), "expect '}' to close the block");
    }
}


static size_t makeJump() {
    emitByte(0xff);
    emitByte(0xff);
    return chunk()->count - 2;
}


static void patchJump(size_t jump) {
    uint8_t *address = chunk()->code + jump;

    size_t jumpTo = chunk()->count - 1;

    address[0] = (jumpTo >> 8) & 0xff;
    address[1] = jumpTo & 0xff;
}


static void patchJumpTo(size_t jump, size_t to) {
    uint8_t *address = chunk()->code + jump;
    address[0] = (to >> 8) & 0xff;
    address[1] = to        & 0xff;
}


static size_t compileConditionalBlock(size_t *breakJump, size_t *continueJump) {
    expression();

    emitByte(OP_JUMP_IF_FALSE);
    size_t ifFalseJump = makeJump();

    consume(TOKEN_LEFT_BRACE, "expect '{' after 'if'/'elseif' conditions");

    emitByte(OP_POP);

    beginScope();
    blockStatement(breakJump, continueJump);

    emitByte(OP_JUMP);
    size_t endJump = makeJump();

    emitByte(OP_POP);
    endScope();

    patchJump(ifFalseJump);
    return endJump;
}


static void compileElseBlock(size_t *breakJump, size_t *continueJump) {
    consume(TOKEN_LEFT_BRACE, "expect '{' after 'else' keyword");

    beginScope();
    blockStatement(breakJump, continueJump);
    endScope();
}


static void ifStatement(size_t *breakJump, size_t *continueJump) {
    size_t jumps[64];
    int count = 0;

    jumps[count++] = compileConditionalBlock(breakJump, continueJump);

    for (;match(TOKEN_ELSE_IF);) {
        jumps[count++] = compileConditionalBlock(breakJump, continueJump);
    }

    if (match(TOKEN_ELSE)) {
        compileElseBlock(breakJump, continueJump);
    }

    emitByte(OP_NOP);

    for (int i = 0; i < count; i++) {
        patchJump(jumps[i]);
    }
}


static bool compileInfiniteLoop() {
    if (!match(TOKEN_LEFT_BRACE)) {
        return false;
    }

    size_t breakJump = 0;
    size_t continueJump = 0;

    size_t jumpTo = chunk()->count;

    blockStatement(&breakJump, &continueJump);

    emitByte(OP_JUMP);
    size_t startJump = makeJump();

    patchJumpTo(startJump, jumpTo);
    if (continueJump) patchJumpTo(continueJump, jumpTo);

    emitByte(OP_NOP);

    if (breakJump) {
        patchJump(breakJump);
    }

    return true;
}


static bool compileWhileLoop() {
    Token *token = current();
    for (;token->type != TOKEN_LEFT_BRACE;) {
        if (token->type == TOKEN_SEMICOLON) return false;
        if (token->type == TOKEN_EOF) errorAtPrevious("'for' signature is wrong");
        token++;
    }

    size_t breakJump = 0;
    size_t continueJump = 0;

    size_t jumpTo = chunk()->count;

    expression();

    emitByte(OP_JUMP_IF_FALSE);
    size_t ifFalseJump = makeJump();

    consume(TOKEN_LEFT_BRACE, "expect '{' after 'for' expression in 'while' like signature");

    emitByte(OP_POP);

    blockStatement(&breakJump, &continueJump);

    emitByte(OP_JUMP);
    size_t startJump = makeJump();

    patchJumpTo(startJump, jumpTo);
    if (continueJump) patchJumpTo(continueJump, jumpTo);

    emitByte(OP_POP);
    patchJump(ifFalseJump);

    if (breakJump) {
        emitByte(OP_NOP);
        patchJump(breakJump);
    }

    return true;
}


static bool compileForLoop() {
    if (!match(TOKEN_SEMICOLON)) {
        expression();
        consume(TOKEN_SEMICOLON, "expect ';' after 'for' initializer expression");
    }

    size_t breakJump = 0;
    size_t continueJump = 0;

    size_t jumpToStart = chunk()->count;
    size_t ifFalseJump = 0;
    size_t ifTrueJump = 0;

    if (!match(TOKEN_SEMICOLON)) {
        expression();
        emitByte(OP_JUMP_IF_FALSE);
        ifFalseJump = makeJump();

        emitByte(OP_JUMP);
        ifTrueJump = makeJump();

        consume(TOKEN_SEMICOLON, "expect ';' after 'for' condition expression");
    }

    size_t jumpToStep = 0;
    if (!match(TOKEN_LEFT_BRACE)) {
        jumpToStep = chunk()->count;
        expression();
        emitByte(OP_POP);
        if (ifFalseJump && ifTrueJump) {
            emitByte(OP_JUMP);
            size_t startJump = makeJump();
            patchJumpTo(startJump, jumpToStart);
        }
        consume(TOKEN_LEFT_BRACE, "expect '{' after 'for' step expression");
    }


    if (ifTrueJump) {
        emitByte(OP_POP);
        patchJump(ifTrueJump);
    }


    blockStatement(&breakJump, &continueJump);

    emitByte(OP_JUMP);
    size_t startJump = makeJump();
    patchJumpTo(startJump, jumpToStep ? jumpToStep : jumpToStart);
    if (continueJump) patchJumpTo(continueJump, jumpToStep ? jumpToStep : jumpToStart);


    if (ifFalseJump) {
        emitByte(OP_POP);
        patchJump(ifFalseJump);
    }

    if (breakJump) {
        emitByte(OP_NOP);
        patchJump(breakJump);
    }

    return true;
}


static void forStatement() {
    beginScope();

    compiler.loopDepth++;

    (void)( compileInfiniteLoop() ||
            compileWhileLoop() ||
            compileForLoop());


    compiler.loopDepth--;

    endScope();
}


static void breakStatement(size_t *breakJump) {
    consume(TOKEN_SEMICOLON, "expect ';' after 'break'");
    emitByte(OP_JUMP);
    *breakJump = makeJump();
}


static void continueStatement(size_t *continueJump) {
    consume(TOKEN_SEMICOLON, "expect ';' after 'continue'");
    emitByte(OP_JUMP);
    *continueJump = makeJump();
}


static void funcStatement() {

}


static void returnStatement() {

}


static void structStatement() {

}


static void statement(size_t *breakJump, size_t *continueJump) {
    switch (forward()->type) {
        case TOKEN_PRINT: return printStatement();
        case TOKEN_PRINTLN: return printlnStatement();
        case TOKEN_VAR: return varStatement();
        case TOKEN_LEFT_BRACE: {
            beginScope();
            blockStatement(breakJump, continueJump);
            endScope();
            return;
        }
        case TOKEN_IF: return ifStatement(breakJump, continueJump);
        case TOKEN_FOR: return forStatement();
        case TOKEN_BREAK: return breakStatement(breakJump);
        case TOKEN_CONTINUE: return continueStatement(continueJump);
        case TOKEN_FUNC: return funcStatement();
        case TOKEN_RETURN: return returnStatement();
        case TOKEN_STRUCT: return structStatement();
        default: {
            backward();
            return expressionStatement();
        }
    }
}


static void statements() {
    for (;!isAtEnd();) {
        statement(NULL, NULL);
    }
}


static Rule rules[] = {
    [TOKEN_IDENTIFIER]      = { identifier, NULL, PRECEDENCE_NONE },
    [TOKEN_NUMBER]          = { number, NULL, PRECEDENCE_NONE },
    [TOKEN_STRING]          = { string, NULL, PRECEDENCE_NONE },
    [TOKEN_TRUE]            = { boolean, NULL, PRECEDENCE_NONE },
    [TOKEN_FALSE]           = { boolean, NULL, PRECEDENCE_NONE },
    [TOKEN_NIL]             = { nil, NULL, PRECEDENCE_NONE },
    [TOKEN_PLUS]            = { unary, binary, PRECEDENCE_TERM },
    [TOKEN_MINUS]           = { unary, binary, PRECEDENCE_TERM },
    [TOKEN_STAR]            = { NULL, binary, PRECEDENCE_FACTOR },
    [TOKEN_SLASH]           = { NULL, binary, PRECEDENCE_FACTOR },
    [TOKEN_PERCENT]         = { NULL, binary, PRECEDENCE_FACTOR },
    [TOKEN_STAR_STAR]       = { NULL, power, PRECEDENCE_POWER },
    [TOKEN_AND]             = { NULL, and_, PRECEDENCE_AND },
    [TOKEN_OR]              = { NULL, or_, PRECEDENCE_OR },
    [TOKEN_BANG]            = { unary, NULL, PRECEDENCE_UNARY },
    // [TOKEN_EQUAL]           = { NULL, assign, PRECEDENCE_ASSIGN },
    // [TOKEN_PLUS_EQUAL]      = { NULL, assign, PRECEDENCE_ASSIGN },
    // [TOKEN_MINUS_EQUAL]     = { NULL, assign, PRECEDENCE_ASSIGN },
    // [TOKEN_STAR_EQUAL]      = { NULL, assign, PRECEDENCE_ASSIGN },
    // [TOKEN_SLASH_EQUAL]     = { NULL, assign, PRECEDENCE_ASSIGN },
    // [TOKEN_PERCENT_EQUAL]   = { NULL, assign, PRECEDENCE_ASSIGN },
    // [TOKEN_STAR_STAR_EQUAL] = { NULL, assign, PRECEDENCE_ASSIGN },
    [TOKEN_EQUAL_EQUAL]     = { NULL, binary, PRECEDENCE_EQUALITY },
    [TOKEN_BANG_EQUAL]      = { NULL, binary, PRECEDENCE_EQUALITY },
    [TOKEN_LESSER]          = { NULL, binary, PRECEDENCE_COMPARISON },
    [TOKEN_LESSER_EQUAL]    = { NULL, binary, PRECEDENCE_COMPARISON },
    [TOKEN_GREATER]         = { NULL, binary, PRECEDENCE_COMPARISON },
    [TOKEN_GREATER_EQUAL]   = { NULL, binary, PRECEDENCE_COMPARISON },
    [TOKEN_COMMA]           = { NULL, binary, PRECEDENCE_COMMA },
    [TOKEN_QUESTION]        = { NULL, ternary, PRECEDENCE_TERNARY },
    [TOKEN_LEFT_PAREN]      = { grouping, call, PRECEDENCE_CALL },
    [TOKEN_RIGHT_PAREN]     = { NULL, NULL, PRECEDENCE_NONE },
    [TOKEN_DOT]             = { NULL, call, PRECEDENCE_CALL },
    [TOKEN_LEFT_BRACE]      = { NULL, NULL, PRECEDENCE_NONE },
    [TOKEN_RIGHT_BRACE]     = { NULL, NULL, PRECEDENCE_NONE },
    [TOKEN_SEMICOLON]       = { NULL, NULL, PRECEDENCE_NONE },
    [TOKEN_COLON]           = { NULL, NULL, PRECEDENCE_NONE },
};


bool compile(const char *source, Chunk *chunk) {
    Tokens tokens;
    initTokens(&tokens);

    if (!getTokens(source, &tokens)) {
        freeTokens(&tokens);
        return false;
    }

    initCompiler(tokens.tokens, chunk);

    statements();

    emitByte(OP_EXIT);
    bool result = !hadErrors();

    freeCompiler();
    freeTokens(&tokens);
    return result;
}
