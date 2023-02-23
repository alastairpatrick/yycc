add_executable(yyct
    Tokenizer.cpp
    "main.cpp"
)

target_link_libraries(yyct PRIVATE lang_lib reflex_lib)
