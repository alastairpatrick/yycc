#include "InternedString.h"
#include "TranslationUnitContext.h"

static string_view empty;
InternedString empty_interned_string = &empty;

InternedString intern_string(string_view source) {
    auto& views = TranslationUnitContext::it->interned_views;
    auto it = views.find(source);
    if (it != views.end()) return &*it;

    auto& strings = TranslationUnitContext::it->interned_strings;
    strings.push_back(string(source));

    auto& inserted = strings.back();
    return &*views.insert(string_view(inserted.c_str())).first;
}
