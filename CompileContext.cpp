#include "CompileContext.h"

#include "std.h"

thread_local CompileContext* CompileContext::it;

CompileContext::CompileContext(ostream& message_stream): message_stream(message_stream) {
    assert(!it);
    it = this;
}

CompileContext::~CompileContext() {
    assert(it == this);
    it = nullptr;
}

ostream& message(const Location& location) {
    return CompileContext::it->message_stream << location.file << ':' << location.line << ':' << location.column << ": ";
}

const string* intern(string source) {
    auto& strings = CompileContext::it->interned_strings;
    return &*strings.insert(move(source)).first;
}
