#include "CompileContext.h"

#include "std.h"

thread_local CompileContext* CompileContext::it;

CompileContext::CompileContext() {
    assert(!it);
    it = this;
}

CompileContext::~CompileContext() {
    assert(it == this);
    it = nullptr;
}
