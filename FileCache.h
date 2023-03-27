#ifndef FILE_CACHE_H
#define FILE_CACHE_H

struct File {
    File() = default;
    explicit File(const string& text);
    void operator=(const File&) = delete;

    string text;
};

struct FileCache {
    static FileCache* it;

    explicit FileCache(bool access_file_system);
    ~FileCache();

    const File* read(const string& path);

    const bool access_file_system;
    unordered_map<string, File> files;
};

#endif
