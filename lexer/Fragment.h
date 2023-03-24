#ifndef LEXER_FRAGMENT_H
#define LEXER_FRAGMENT_H

struct Fragment {
    Fragment() = default;
    Fragment(size_t position, size_t length): position(position), length(length) {}

    size_t position{};
    size_t length{};

    string_view text() const;

    static Fragment context();
};

#endif
