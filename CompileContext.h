#ifndef COMPILE_CONTEXT_H
#define COMPILE_CONTEXT_H

#include "TypeContext.h"

struct CompileContext {
    static thread_local CompileContext* it;

    CompileContext();
    ~CompileContext();

    TypeContext type;
};

#endif
