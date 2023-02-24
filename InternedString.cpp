#include "InternedString.h"
#include "CompileContext.h"

static string_view empty;
InternedString EmptyInternedString = &empty;

InternedString InternString(string_view source) {
    auto& views = CompileContext::it->interned_views;
    auto it = views.find(source);
    if (it != views.end()) return &*it;

    auto& strings = CompileContext::it->interned_strings;
    strings.push_back(string(source));

    auto& inserted = strings.back();
    return &*views.insert(string_view(inserted)).first;
}
