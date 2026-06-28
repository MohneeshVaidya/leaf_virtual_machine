#include "compiler.h"
#include "tokenizer.h"


bool compile(const char *source, Chunk *chunk) {
    Tokens tokens;
    initTokens(&tokens);

    if (!getTokens(source, &tokens)) {
        freeTokens(&tokens);
        return false;
    }

    freeTokens(&tokens);
    return true;
}
