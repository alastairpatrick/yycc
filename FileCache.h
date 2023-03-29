#ifndef FILE_CACHE_H
#define FILE_CACHE_H

struct File {
    bool exists{};
    string text;
    filesystem::path path;
};

struct Directory {
    bool system{};
    filesystem::path path;
};

struct FileCache {
    static FileCache* it;

    explicit FileCache(bool access_file_system);
    ~FileCache();

    File read(const filesystem::path& path);

    void add_search_directory(bool system, string_view path);
    const File* search(string_view header_name);

    File* add(string_view header_name);

    const bool access_file_system;

    vector<Directory> directories;
    list<string> header_names;
    unordered_map<string_view, File> files;
};

#endif
