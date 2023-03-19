add_executable(yycpp
    "yycpp.cpp"
)

target_link_libraries(yycpp PRIVATE lang_lib reflex_lib)
