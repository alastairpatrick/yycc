cmake_minimum_required(VERSION 3.20.6)

project(lang)

set(CMAKE_CXX_STANDARD 17)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")

add_library(lang_lib STATIC
    assoc_prec.c
    ASTNode.cpp
    CompileContext.cpp
    Constant.cpp
    Decl.cpp
    Expr.cpp
    InternedString.cpp
    Lexer.cpp
    Parser.cpp
    Printable.cpp
    Statement.cpp
    SymbolMap.cpp
    Type.cpp
    TypeContext.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/PPTokenLexer.yy.cpp
)

add_custom_command(
    DEPENDS reflex Lexer.l
    OUTPUT  lex.yy.cpp lex.yy.h
    COMMAND reflex --header-file
            ${CMAKE_CURRENT_SOURCE_DIR}/Lexer.l
)

add_custom_command(
    DEPENDS reflex preprocessor/PPTokenLexer.l
    OUTPUT  PPTokenLexer.yy.cpp PPTokenLexer.yy.h
    COMMAND reflex --header-file=PPTokenLexer.yy.h --outfile=PPTokenLexer.yy.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/preprocessor/PPTokenLexer.l
)

target_include_directories(lang_lib
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}
        third_party
        third_party/reflex/include
        .
        ${LLVM_INCLUDE_DIRS}
)

target_compile_definitions(lang_lib PRIVATE
    ${LLVM_DEFINITIONS}
)

llvm_map_components_to_libnames(lang_llvm_libs core executionengine mcjit interpreter analysis native bitwriter ${LLVM_ALL_TARGETS})
target_link_libraries(lang_lib PRIVATE reflex_lib nlohmann_json::nlohmann_json ${lang_llvm_libs})

add_executable(lang
    main.cpp
)

target_link_libraries(lang PRIVATE lang_lib)

add_subdirectory(test)
add_subdirectory(third_party)
add_subdirectory(yyct)
