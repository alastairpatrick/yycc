#include "FileCache.h"

#include "Message.h"

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

File FileCache::read(const filesystem::path& path) {
    filesystem::path preferred(path);
    preferred.make_preferred();

#ifdef _WIN32
    auto file = _wfopen(preferred.c_str(), L"rb");
#else
    auto file = fopen(preferred.c_str(), "rb");
#endif
    File result;
    if (file) {
        result.exists = true;
        result.path = path;
        result.text = splice_physical_lines(Input(file));
        fclose(file);
    }
    return result;
}

void FileCache::add_search_directory(bool system, string_view path) {
    directories.push_back(Directory { system, path });
}

const File* FileCache::search(string_view header_name) {
    auto it = files.find(header_name);
    if (it != files.end()) return &it->second;

    if (access_file_system) {
        auto system_only = header_name.front() == '<';

        auto filename = header_name;
        filename.remove_prefix(1);
        filename.remove_suffix(1);

        for (auto& directory : directories) {
            if (system_only && !directory.system) continue;

            auto path = directory.path / filename;
            auto file = read(path);
            if (file.exists) {
                auto perm_file = add(header_name);
                *perm_file = move(file);
                return perm_file;
            }
        }
    }

    return nullptr;
}

File* FileCache::add(string_view header_name) {
    header_names.push_back(string(header_name));
    auto& str = header_names.back();
    auto& result = files[str];
    return &result;
}
