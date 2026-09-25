#include "n64stack.h"

// Per thread: it only lives during a step.
__thread uint32_t gN64StackPointer __attribute__((tls_model("initial-exec")));
