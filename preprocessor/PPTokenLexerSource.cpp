#include "PPTokenLexerSource.h"
  
PPTokenLexerSource::PPTokenLexerSource(const reflex::Input& input): lexer(input) {}

void PPTokenLexerSource::set_filename(const string& filename) {
    current_filename = filenames.insert(filename).first->c_str();
}
