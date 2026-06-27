#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>


#include "vm.h"


void runRepl() {

}


const char *readFile(const char *filePath) {
    FILE *file = fopen(filePath, "rb");
    if (file == NULL) {
        fprintf(stderr, "not able to open file \"%s\"\n", filePath);
        exit(64);
    }

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char *buffer = (char *)malloc(fileSize + 1);
    if (buffer == NULL) {
        fprintf(stderr, "not able to allocate memory for \"%s\"\n", filePath);
        fclose(file);
        exit(70);
    }

    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < fileSize) {
        fprintf(stderr, "not able to read file \"%s\"\n", filePath);
        free(buffer);
        fclose(file);
        exit(70);
    }
    buffer[fileSize] = '\0';

    fclose(file);

    return buffer;
}


void runFile(const char *filePath) {
    const char *source = readFile(filePath);

    InterpretResult result = interpret(source);

    if (result == INTERPRET_COMPILE_ERROR) exit(74);
    if (result == INTERPRET_RUNTIME_ERROR) exit(75);
}


int main(int argc, char *argv[]) {
    if (argc == 1) {
        runRepl();
    } else if (argc == 2) {
        runFile(argv[1]);
    } else {
        fprintf(stderr, "usage: leaf <filePath>\n");
        exit(1);
    }

    return 0;
}
