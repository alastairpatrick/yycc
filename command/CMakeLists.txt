add_executable(yycpp
    "yycpp.cpp"
)

target_link_libraries(yycpp PRIVATE lang_lib reflex_lib)


add_executable(yyc
    "yyc.cpp"
)

target_link_libraries(yyc PRIVATE lang_lib reflex_lib)
