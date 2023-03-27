#include "FileCache.h"

#include "Message.h"

File::File(const string& text): text(text) {
}

FileCache* FileCache::it;

FileCache::FileCache(bool access_file_system): access_file_system(access_file_system) {
    assert(!it);
    it = this;
}

FileCache::~FileCache() {
    assert(it == this);
    it = nullptr;
}

// This applies translation phases 1 & 2 described in the C standard: remapping character set and removing backslash-newline sequences.
static string splice_physical_lines(const Input& input) {
    Input in(input);

    string text;
    bool backslash{};
    for (;;) {
        char buffer[0x10000];
        auto bytes_read = in.get(buffer, sizeof(buffer));
        if (bytes_read == 0) break;

        size_t dest = 0;
        size_t insert_count = 0;
        for (size_t src = 0; src < bytes_read; ++src) {
            char c = buffer[src];
            if (backslash) {
                if (c == '\n') {
                    ++insert_count;
                } else {
                    buffer[dest++] = '\\';
                    buffer[dest++] = c;
                }
                backslash = false;
            } else if (c == '\\') {
                backslash = true;
            } else {
                if (c == '\n') {
                    // Whenever a backslash & new-line character pair is deleted, insert a new-line immediately before the
                    // next new-line character that is not preceeded by a backslash character. This keeps logical line
                    // numbers eual to physical line numbers.
                    for (; insert_count; --insert_count) {
                        buffer[dest++] = c;
                    }
                }

                buffer[dest++] = c;
            }
        }

        text.append(buffer, dest);
    }
    text += '\n';

    return text;
}

const File* FileCache::read(const string& path) {
    auto it = files.find(path);
    if (it != files.end()) return &it->second;

    if (access_file_system) {
        auto file = fopen(path.c_str(), "rb");
        if (file) {
            auto& result = files[path];
            result.text = splice_physical_lines(Input(file));
            fclose(file);
            return &result;
        }
    }

    return nullptr;
}
