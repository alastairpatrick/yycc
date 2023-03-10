#include "PPTokenLexerSource.h"
  
PPTokenLexerSource::PPTokenLexerSource(const reflex::Input& input): lexer(input) {}

void PPTokenLexerSource::set_filename(string&& filename) {
    auto& str = *filenames.insert(move(filename)).first;
    current_filename = string_view(str.data(), str.length());
}
