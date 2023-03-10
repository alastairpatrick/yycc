cmake_minimum_required(VERSION 3.20.6)

project(lang)

set(CMAKE_CXX_STANDARD 17)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")

add_library(lang_lib_c STATIC
    assoc_prec.c
)

add_library(lang_lib STATIC
    ASTNode.cpp
    CompileContext.cpp
    Constant.cpp
    Declaration.cpp
    Expr.cpp
    InternedString.cpp
    Message.cpp
    Parser.cpp
    Printable.cpp
    Statement.cpp
    SymbolMap.cpp
    Type.cpp
    TypeContext.cpp

    preprocessor/PPTokenLexerSource.cpp
    preprocessor/TokenConverter.cpp

    ${CMAKE_CURRENT_BINARY_DIR}/PPNumberLexer.yy.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/PPTokenLexer.yy.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/IdentifierLexer.yy.cpp
)

target_precompile_headers(lang_lib PUBLIC "pch.h")

add_custom_command(
    DEPENDS reflex preprocessor/PPTokenLexer.l
    OUTPUT  PPTokenLexer.yy.cpp PPTokenLexer.yy.h
    COMMAND reflex --header-file=PPTokenLexer.yy.h --outfile=PPTokenLexer.yy.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/preprocessor/PPTokenLexer.l
)

add_custom_command(
    DEPENDS reflex preprocessor/IdentifierLexer.l
    OUTPUT  IdentifierLexer.yy.cpp IdentifierLexer.yy.h
    COMMAND reflex --header-file=IdentifierLexer.yy.h --outfile=IdentifierLexer.yy.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/preprocessor/IdentifierLexer.l
)

add_custom_command(
    DEPENDS reflex preprocessor/PPNumberLexer.l
    OUTPUT  PPNumberLexer.yy.cpp PPNumberLexer.yy.h
    COMMAND reflex --header-file=PPNumberLexer.yy.h --outfile=PPNumberLexer.yy.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/preprocessor/PPNumberLexer.l
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

target_link_libraries(lang_lib PRIVATE
    lang_lib_c
    reflex_lib
    nlohmann_json::nlohmann_json
    ${lang_llvm_libs}
)

add_executable(lang
    main.cpp
)

target_link_libraries(lang PRIVATE lang_lib)

add_subdirectory(test)
add_subdirectory(third_party)
add_subdirectory(yyct)
