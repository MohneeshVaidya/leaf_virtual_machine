#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>


#include "compiler.h"
#include "chunk.h"
#include "forward.h"
#include "object.h"
#include "table.h"
#include "tokenizer.h"
#include "value.h"
#include "vm.h"


typedef struct Parser {
    Token *tokens;
    Token *current;
    Token *previous;
} Parser;


struct Local {
    ObjString *name;
    int depth;
};


typedef struct Compiler {
    struct Compiler *enclosing;
    ObjClosure *closure;
    Local locals[256];
    int localCount;
    int scopeDepth;
    bool hadErrors;
    int loopDepth;
    int functionDepth;
} Compiler;


static Parser parser;
static Compiler *currentCompiler = NULL;


static void initParser(Token *tokens) {
    parser.tokens = tokens;
    parser.current = parser.tokens;
    parser.previous = NULL;
}


static void freeParser() {
    initParser(NULL);
}


static void startCompiler(Compiler *enclosing) {
    currentCompiler->enclosing = enclosing;
    currentCompiler->closure = makeClosure();
    currentCompiler->localCount = 0;
    currentCompiler->scopeDepth = 0;
    currentCompiler->hadErrors = false;
    currentCompiler->loopDepth = 0;
    currentCompiler->functionDepth = 0;
}


static ObjClosure *endCompiler() {
    return currentCompiler->hadErrors ? NULL : currentCompiler->closure;
}


static Token *tokens() { return parser.tokens; }
static Token *current() { return parser.current; }
static Token *previous() { return parser.previous; }
static ObjClosure *closure() { return currentCompiler->closure; }
static Chunk *chunk() { return &closure()->function->chunk; }
static Local *locals() { return currentCompiler->locals; }
static int localCount() { return currentCompiler->localCount; }
static int scopeDepth() { return currentCompiler->scopeDepth; }
static bool hadErrors() { return currentCompiler->hadErrors; }
static int loopDepth() { return currentCompiler->loopDepth; }
static int functionDepth() { return currentCompiler->functionDepth; }


static void errorAt(Token *token, const char *format, ...) {
    currentCompiler->hadErrors = true;

    va_list args;
    va_start(args, format);

    fprintf(stderr, "compile error: [at line %d], ", token->line);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");

    va_end(args);
}


// static void errorAt(Token *token, const char *errorMessage) {
//     currentCompiler->hadErrors = true;
//     fprintf(stderr, "compile error: [at line %d], %s\n", token->line, errorMessage);
// }


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
    parser.previous = current();

    if (isAtEnd()) return current();

    return parser.current++;
}


static Token *backward() {
    if (current() == tokens()) {
        return NULL;
    } else if (current() - tokens() > 1) {
        parser.previous = current() - 2;
    }
    return parser.current--;
}


static bool check(TokenType type) {
    return current()->type == type;
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
    if (localCount() >= 256) {
        errorAt(previous(), "local's pool overflow");
        return -1;
    }

    ObjString *name = AS_STRING(chunk()->constantPool[index]);

    for (int i = localCount() - 1; (i > -1) && (locals()[i].depth == scopeDepth()); i-- ) {
        if (name == locals()[i].name) {
            errorAt(previous(), "redeclaring '%s' in same scope", name->chars);
            return -1;
        }
    }

    locals()[localCount()].name = name;
    locals()[localCount()].depth = scopeDepth();
    return currentCompiler->localCount++;
}


typedef enum ResolvedType {
    RESOLVED_LOCAL,
    RESOLVED_UPVALUE,
    RESOLVED_GLOBAL,
} ResolvedType;


typedef struct ResolvedInformation {
    ResolvedType type;
    int index;
} ResolvedInformation;


static void printLocals(Compiler *compiler, int height) {
    printf("compiler's height: %d - ", height);
    if (!compiler) {
        printf("compiler is NULL\n");
        return;
    }
    for (int i = 0; i < compiler->localCount; i++) {
        printObject((Obj*)compiler->locals[i].name);
        printf(" , ");
    }
    printf("\n");
}


static int addUpvalue(ObjClosure *closure, ObjString *name, int height, int index) {
    int length = closure->upvalueCount;
    UpValue *upvalues = closure->upvalues;

    for (int i = 0; i < length; i++) {
        if (upvalues->name == name) {
            return i;
        }
    }

    upvalues[length].name = name;
    upvalues[length].height = height;
    upvalues[length].index = index;
    return closure->upvalueCount++;
}


static ResolvedInformation resolveInCompiler(Compiler *compiler, ObjString *name, int index, int height) {
    if (compiler == NULL) {
        return (ResolvedInformation){ RESOLVED_GLOBAL, index };
    }

    for (int i = compiler->localCount - 1; i > -1; i--) {
        if (name == compiler->locals[i].name) {
            return (ResolvedInformation){ RESOLVED_UPVALUE, addUpvalue(closure(), name, height, i) };
        }
    }

    return resolveInCompiler(compiler->enclosing, name, index, height + 1);
}


static ResolvedInformation resolveLocal(int index) {
    ObjString *name = AS_STRING(chunk()->constantPool[index]);

    for (int i = localCount() - 1; i > -1; i--) {
        if (name == locals()[i].name) {
            setDebugLocal(chunk(), i, name);
            return (ResolvedInformation){ RESOLVED_LOCAL, i  };
        }
    }

    return resolveInCompiler(currentCompiler->enclosing, name, index, 0);
}


static void addGlobal(int index) {
    ObjString *name = AS_STRING(chunk()->constantPool[index]);

    if (tableContains(&vm.globals, name)) {
        errorAt(previous(), "redeclaring '%s' in same scope", name->chars);
        return;
    }

    tableSet(&vm.globals, name, NIL_VALUE);
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
static void call();


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

    ResolvedInformation information = resolveLocal(index);

    Operation opGet;
    Operation opSet;

    if (information.type == RESOLVED_GLOBAL) {
        opGet = OP_GET_GLOBAL;
        opSet = OP_SET_GLOBAL;
    } else if (information.type == RESOLVED_LOCAL) {
        opGet = OP_GET_LOCAL;
        opSet = OP_SET_LOCAL;
    } else {
        opGet = OP_GET_UPVALUE;
        opSet = OP_SET_UPVALUE;
    }
    if (!assign(information.index, opGet, opSet)) {
        emitBytes(opGet, information.index);

        if (match(TOKEN_LEFT_PAREN)) {
            call();
        }
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
    int arity = 0;
    for (;!isAtEnd() && !check(TOKEN_RIGHT_PAREN);) {
        parsePrecedence(PRECEDENCE_COMMA + 1);
        arity++;
        if (check(TOKEN_RIGHT_PAREN)) {
            break;
        }
        consume(TOKEN_COMMA, "expect ',' between function arguments");
    }
    consume(TOKEN_RIGHT_PAREN, "expect ')' after argument list");

    emitBytes(OP_CALL, arity);

    for (;match(TOKEN_LEFT_PAREN);) {
        call();
    }
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
    if (scopeDepth() > 0) {
        addLocal(index);
        operation = OP_DECLARE_LOCAL;
    } else {
        addGlobal(index);
        operation = OP_DECLARE_GLOBAL;
    }

    emitBytes(operation, index);
}


static void beginScope() {
    currentCompiler->scopeDepth++;
}


static void endScope() {
    currentCompiler->scopeDepth--;

    int i = localCount();
    for (;i > 0 && locals()[i-1].depth > scopeDepth(); i--) {
        emitByte(OP_POP);
    }
    currentCompiler->localCount = i;
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
    emitBytes(0xff, 0xff);
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
    endScope();

    emitByte(OP_JUMP);
    size_t endJump = makeJump();

    emitByte(OP_POP);
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
    size_t jumps[1024];
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
        emitByte(OP_POP);
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

    currentCompiler->loopDepth++;

    (void)( compileInfiniteLoop() ||
            compileWhileLoop() ||
            compileForLoop());


    currentCompiler->loopDepth--;

    endScope();
}


static void breakStatement(size_t *breakJump) {
    if (loopDepth() == 0) {
        errorAtPrevious("'break' statement can be used only within the loop context");
        return;
    }
    consume(TOKEN_SEMICOLON, "expect ';' after 'break'");
    emitByte(OP_JUMP);
    *breakJump = makeJump();
}


static void continueStatement(size_t *continueJump) {
    if (loopDepth() == 0) {
        errorAtPrevious("'continue' statement can be used only within the loop context");
        return;
    }
    consume(TOKEN_SEMICOLON, "expect ';' after 'continue'");
    emitByte(OP_JUMP);
    *continueJump = makeJump();
}


static void parseParameters() {
    consume(TOKEN_LEFT_PAREN, "expect '(' after function name");

    for (;!isAtEnd() && !check(TOKEN_RIGHT_PAREN);) {
        if (closure()->function->arity >= 256) {
            errorAtPrevious("number of functions parameters can not exceed 256");
        }

        int index = parseIdentifier();
        closure()->function->parameters[closure()->function->arity] = AS_STRING(chunk()->constantPool[index]);
        closure()->function->arity++;
        addLocal(index);
        emitBytes(OP_DECLARE_LOCAL, index);
        if (check(TOKEN_RIGHT_PAREN)) {
            break;
        }
        consume(TOKEN_COMMA, "expect ',' between function parameters");
    }

    consume(TOKEN_RIGHT_PAREN, "expect ')' after the argument list");
}


static int declareFunction(int index) {
    emitBytes(OP_CONSTANT, makeConstant(NIL_VALUE));

    int localIndex = -1;

    Operation operation ;
    if (scopeDepth() > 0) {
        localIndex = addLocal(index);
        operation = OP_DECLARE_LOCAL;
    } else {
        addGlobal(index);
        operation = OP_DECLARE_GLOBAL;
    }
    emitBytes(operation, index);
    return localIndex;
}


static void initializeFunction(int index) {
    if (scopeDepth() > 0) {
        emitBytes(OP_CLONE_CLOSURE, index);
        emitByte(OP_POP);
    } else {
        emitBytes(OP_SET_GLOBAL, index);
        emitByte(OP_POP);
    }
}


static void funcStatement() {
    int index = parseIdentifier();
    int localIndex = declareFunction(index);

    ObjString *name = AS_STRING(chunk()->constantPool[index]);

    Compiler *previousCompiler = currentCompiler;
    Compiler compiler;
    currentCompiler = &compiler;
    startCompiler(previousCompiler);

    closure()->function->name = name;

    beginScope();

    parseParameters();

    consume(TOKEN_LEFT_BRACE, "expect '{' after ')' in function declaration");


    currentCompiler->functionDepth++;
    blockStatement(NULL, NULL);
    currentCompiler->functionDepth--;

    endScope();

    emitBytes(OP_CONSTANT, makeConstant(NIL_VALUE));
    emitByte(OP_RETURN);

    ObjClosure *closure = endCompiler();
    if (!closure) return;

    currentCompiler = currentCompiler->enclosing;

    emitBytes(OP_CONSTANT, makeConstant(OBJ_VALUE(closure)));

    initializeFunction(localIndex == -1 ? index : localIndex);
    // Operation operation ;
    // if (scopeDepth() > 0) {
    //     addLocal(index);
    //     operation = OP_DECLARE_LOCAL;
    // } else {
    //     addGlobal(index);
    //     operation = OP_DECLARE_GLOBAL;
    // }
    // emitBytes(operation, index);

}


static void returnStatement() {
    if (functionDepth() == 0) {
        errorAtPrevious("'return' statement can be used only within the function's context");
        return;
    }
    if (check(TOKEN_SEMICOLON)) {
        emitBytes(OP_CONSTANT, makeConstant(NIL_VALUE));
    } else {
        expression();
    }
    consume(TOKEN_SEMICOLON, "expect ';' after 'return' statement");
    emitByte(OP_RETURN);
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
    [TOKEN_LEFT_PAREN]      = { grouping, NULL, PRECEDENCE_CALL },
    [TOKEN_RIGHT_PAREN]     = { NULL, NULL, PRECEDENCE_NONE },
    // [TOKEN_DOT]             = { NULL, call, PRECEDENCE_CALL },
    [TOKEN_LEFT_BRACE]      = { NULL, NULL, PRECEDENCE_NONE },
    [TOKEN_RIGHT_BRACE]     = { NULL, NULL, PRECEDENCE_NONE },
    [TOKEN_SEMICOLON]       = { NULL, NULL, PRECEDENCE_NONE },
    [TOKEN_COLON]           = { NULL, NULL, PRECEDENCE_NONE },
};


ObjClosure *compile(const char *source) {
    Tokens tokens;
    initTokens(&tokens);

    if (!getTokens(source, &tokens)) {
        freeTokens(&tokens);
        return false;
    }

    initParser(tokens.tokens);

    Compiler compile;
    currentCompiler = &compile;

    startCompiler(NULL);

    statements();
    emitByte(OP_EXIT);
    bool result = !hadErrors();

    freeParser();
    freeTokens(&tokens);
    return endCompiler();
}
