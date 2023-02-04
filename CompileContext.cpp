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

const string* intern(string source) {
    auto& strings = CompileContext::it->interned_strings;
    return &*strings.insert(move(source)).first;
}
