#include "Fragment.h"

#include "Context.h"

string_view Fragment::text() const {
    auto& input = Context::it->lexer.input;
    return string_view(input.data() + position, length);
}

Fragment Fragment::context() {
    auto& input = Context::it->lexer.input;
    return Fragment(0, input.length());
}
