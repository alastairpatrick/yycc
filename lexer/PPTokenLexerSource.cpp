#include "PPTokenLexerSource.h"

void PPTokenLexerSource::set_filename(string&& filename) {
    auto& str = *filenames.insert(move(filename)).first;
    current_filename = string_view(str.data(), str.length());
}
