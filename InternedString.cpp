#include "InternedString.h"
#include "TranslationUnitContext.h"

static string_view empty;
InternedString empty_interned_string = &empty;

size_t InternedStringPairHash::operator()(const InternedStringPair& pair) const {
    hash<string_view> h;
    return h(pair.first) + h(pair.second);
}

bool InternedStringPairKeyEqual::operator()(const InternedStringPair& a, const InternedStringPair& b) const {
    size_t a_length = a.first.length() + a.second.length();
    size_t b_length = b.first.length() + b.second.length();
    if (a_length != b_length) return false;

    for (size_t i = 0; i < a_length; ++i) {
        char ac = i < a.first.length() ? a.first[i] : a.second[i - a.first.length()];
        char bc = i < b.first.length() ? b.first[i] : b.second[i - b.first.length()];
        if (ac != bc) return false;
    }

    return true;
}

InternedString intern_string(string_view first, string_view second) {
    size_t length = first.length() + second.length();
    if (length == 0) return &empty;

    auto& views = TranslationUnitContext::it->interned_views;

    InternedStringPair key{ first, second };
    auto it = views.find(key);
    if (it != views.end()) return &it->first;

    auto& strings = TranslationUnitContext::it->interned_strings;
    string buffer;
    buffer.reserve(length + 1);
    buffer += first;
    buffer += second;
    buffer += '\0';
    strings.push_back(move(buffer));

    auto& inserted = strings.back();
    return &views.insert(make_pair(string_view(inserted.data()), string_view())).first->first;
}

const char* c_str(InternedString str) {
    if (str->empty()) return "";
    return str->data();
}
