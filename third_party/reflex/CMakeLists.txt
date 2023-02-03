add_library(reflex_lib STATIC
    lib/convert.cpp
    lib/debug.cpp
    lib/error.cpp
    lib/input.cpp
    lib/matcher.cpp
    lib/pattern.cpp
    lib/posix.cpp
    lib/unicode.cpp
    lib/utf8.cpp
    unicode/block_scripts.cpp
    unicode/language_scripts.cpp
    unicode/letter_scripts.cpp
)

target_include_directories(reflex_lib PUBLIC
    include
)

add_executable(reflex
    src/reflex.cpp
)

target_link_libraries(reflex PRIVATE reflex_lib)
