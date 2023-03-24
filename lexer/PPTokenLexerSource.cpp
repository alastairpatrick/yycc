#include "PPTokenLexerSource.h"

#include "CompileContext.h"

void PPTokenLexerSource::set_input(const Input& input) {
    lexer.in(input);
}

void PPTokenLexerSource::set_input(const Fragment& fragment) {
    auto text = fragment.text();
    lexer.buffer((char*) text.data(), text.size() + 1);
}

void PPTokenLexerSource::set_filename(string&& filename) {
    auto& str = *filenames.insert(move(filename)).first;
    current_filename = string_view(str.data(), str.length());
}
