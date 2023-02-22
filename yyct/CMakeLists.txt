add_library(yyct_lib STATIC
    
    Tokenizer.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/PPLexer.yy.cpp
)

add_custom_command(
    DEPENDS reflex PPLexer.l
    OUTPUT  PPLexer.yy.cpp PPLexer.yy.h
    COMMAND reflex --header-file=PPLexer.yy.h --outfile=PPLexer.yy.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/PPLexer.l
)

target_include_directories(yyct_lib
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(yyct_lib PUBLIC lang_lib reflex_lib)

add_executable(yyct
    main.cpp
)

target_link_libraries(yyct PRIVATE yyct_lib)
