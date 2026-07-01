#ifndef leaf_operation_h
#define leaf_operation_h


#include "chunk.h"


typedef struct OperationResult {
    bool isError;
    const char *errorMessage;
} OperationResult;


OperationResult performBinary(Operation operation);
OperationResult performUnary(Operation operation);
OperationResult performLogical(Operation operation);


#endif
