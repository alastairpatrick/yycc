#include "TranslationUnitContext.h"
#include "lexer/Fragment.h"
#include "FileCache.h"
#include "Message.h"

void sweep(ostream& stream, const File& file);

int main(int argc, const char* argv[]) {
    FileCache file_cache(true);

    Severity highest_severity{};

    for (auto i = 1; i < argc; ++i) {
        TranslationUnitContext context(cerr);

        auto in_file = FileCache::it->read(argv[i]);
        if (!in_file.exists) {
            message(Severity::ERROR, Location{1, 1, intern_string(argv[i])}) << "could not open input file\n";
        } else {
            fstream out_file(string(argv[i]) + "~", ios_base::out | ios_base::trunc | ios_base::binary);

            sweep(out_file, in_file);
        }

        highest_severity = max(highest_severity, context.highest_severity);
    }

    return highest_severity == Severity::INFO ? EXIT_SUCCESS : EXIT_FAILURE;
}

