#include "CompileContext.h"

void sweep(ostream& stream, const string& translation_unit);

static string remap_chars(FILE* file) {
    Input file_input(file);
    string source;
    for (;;) {
        char buffer[0x10000];
        auto bytes_read = file_input.get(buffer, sizeof(buffer));
        if (bytes_read == 0) break;
        source.append(buffer, bytes_read);
    }
    source += '\n';

    return source;
}

static void splice_physical_lines(string& source) {
    auto src = 0;
    auto dest = 0;
    auto insert_count = 0;
    while (src < source.size()) {
        if (source[src] == '\\' && source[src + 1] == '\n') {
            ++insert_count;
            src += 2;
        } else {
            if (source[src] == '\n') {
                // Whenever a backslash & new-line character pair is deleted, insert a new-line immediately before the
                // next new-line character that is not preceeded by a backslash character. This keeps logical line
                // numbers eual to physical line numbers.
                for (; insert_count; --insert_count) {
                    source[dest++] = '\n';
                }
            }
            source[dest++] = source[src++];
        }
    }
    source.resize(dest);
}

int main(int argc, const char* argv[]) {
    CompileContext context(cerr);

    auto errors = 0;

    for (auto i = 1; i < argc; ++i) {
        auto in_file = fopen(argv[i], "rb");
        if (!in_file) {
            fprintf(stderr, "Could not open input file '%s'", argv[i]);
            ++errors;
        } else {
            // Translation phases 1 - 3
            string unit = remap_chars(in_file);
            fclose(in_file);

            splice_physical_lines(unit);

            fstream out_file(string(argv[i]) + "~", ios_base::out | ios_base::trunc | ios_base::binary);
            
            sweep(out_file, unit);
        }
    }
}

