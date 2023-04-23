add_executable(yycpp
    "yycpp.cpp"
)

target_link_libraries(yycpp PRIVATE yycc_lib reflex_lib)


add_executable(yyc
    "yyc.cpp"
)

target_link_libraries(yyc PRIVATE yycc_lib reflex_lib)
