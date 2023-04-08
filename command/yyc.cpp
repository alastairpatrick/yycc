#include "TranslationUnitContext.h"
#include "lexer/Fragment.h"
#include "FileCache.h"
#include "Message.h"
#include "parser/Parser.h"
#include "preprocessor/Preprocessor.h"

void sweep(ostream& stream, const File& file);

struct TranslationInput {
    explicit TranslationInput(IdentifierMap& identifiers, File&& f): file(f), preprocessor(file.text, false), parser(preprocessor, identifiers) {
    }
    void operator=(const TranslationInput&) = delete;

    File file;
    Preprocessor preprocessor;
    Parser parser;
};

int main(int argc, const char* argv[]) {
    FileCache file_cache(true);

    TranslationUnitContext context(cerr);
    IdentifierMap identifiers(false);
    list<TranslationInput> inputs;
    for (auto i = 1; i < argc; ++i) {
        auto in_file = FileCache::it->read(argv[i]);

        if (!in_file.exists) {
            message(Severity::ERROR, Location{1, 1, intern_string(argv[i])}) << "could not open input file\n";
        } else {
            inputs.emplace_back(identifiers, move(in_file));
        }
    }

    ASTNodeVector declarations;
    for (auto& input : inputs) {
        auto unit_declarations = input.parser.parse();
        declarations.insert(declarations.end(), unit_declarations.begin(), unit_declarations.end());
    }

    return context.highest_severity == Severity::INFO ? EXIT_SUCCESS : EXIT_FAILURE;
}

