#include "TranslationUnitContext.h"
#include "lexer/Fragment.h"
#include "FileCache.h"

void sweep(ostream& stream, const File& file);

int main(int argc, const char* argv[]) {
    FileCache file_cache(true);

    auto errors = 0;

    for (auto i = 1; i < argc; ++i) {
        TranslationUnitContext context(cerr);

        auto in_file = FileCache::it->read(argv[i]);
        if (!in_file) {
            fprintf(stderr, "Could not open input file '%s'", argv[i]);
            ++errors;
        } else {
            fstream out_file(string(argv[i]) + "~", ios_base::out | ios_base::trunc | ios_base::binary);

            sweep(out_file, *in_file);
        }
    }
}

