#ifndef FILE_CACHE_H
#define FILE_CACHE_H

struct File {
    explicit File(const string& text);
    void operator=(const File&) = delete;

    File(File&&) = default;
    File& operator=(File&&) = default;

    const string text;
};

struct FileCache {
    static FileCache* it;

    explicit FileCache(bool access_file_system);
    ~FileCache();

    const File* read(const string& path);
    const File* add(const string& path, const Input& input);

private:
    const bool access_file_system;
    unordered_map<string, File> files;
};

#endif
