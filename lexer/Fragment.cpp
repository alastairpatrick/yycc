#include "Fragment.h"

#include "CompileContext.h"

string_view Fragment::text() const {
    auto& input = CompileContext::it->lexer.input;
    return string_view(input.data() + position, length);
}

Fragment Fragment::context() {
    auto& input = CompileContext::it->lexer.input;
    return Fragment(0, input.length());
}
