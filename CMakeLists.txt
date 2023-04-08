cmake_minimum_required(VERSION 3.20.6)

project(lang)

set(CMAKE_CXX_STANDARD 17)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")

set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

add_library(lang_lib_c STATIC
    "parser/AssocPrec.c"
)

add_library(lang_lib STATIC
    "FileCache.cpp"
    "InternedString.cpp"
    "Message.cpp"
    "Printable.cpp"
    "TranslationUnitContext.cpp"

    "lexer/Unescape.cpp"

    "parser/ASTNode.cpp"
    "parser/Constant.cpp"
    "parser/Declaration.cpp"
    "parser/Expr.cpp"
    "parser/IdentifierMap.cpp"
    "parser/Parser.cpp"
    "parser/Statement.cpp"
    "parser/Type.cpp"
    "parser/TypeContext.cpp"

    "preprocessor/Preprocessor.cpp"
    

    "Sweep.cpp"
    "TextStream.cpp"

    "${GENERATED_DIR}/PPNumberLexer.yy.cpp"
    "${GENERATED_DIR}/PPTokenLexer.yy.cpp"
    "${GENERATED_DIR}/IdentifierLexer.yy.cpp"
)

target_precompile_headers(lang_lib PUBLIC "pch.h")

add_custom_command(
    DEPENDS reflex lexer/PPTokenLexer.l
    OUTPUT  "${GENERATED_DIR}/PPTokenLexer.yy.cpp" "${GENERATED_DIR}/PPTokenLexer.yy.h"
    COMMAND reflex "--header-file=${GENERATED_DIR}/PPTokenLexer.yy.h" "--outfile=${GENERATED_DIR}/PPTokenLexer.yy.cpp"
            ${CMAKE_CURRENT_SOURCE_DIR}/lexer/PPTokenLexer.l
)

add_custom_command(
    DEPENDS reflex lexer/IdentifierLexer.l
    OUTPUT  "${GENERATED_DIR}/IdentifierLexer.yy.cpp" "${GENERATED_DIR}/IdentifierLexer.yy.h"
    COMMAND reflex "--header-file=${GENERATED_DIR}/IdentifierLexer.yy.h" "--outfile=${GENERATED_DIR}/IdentifierLexer.yy.cpp"
            ${CMAKE_CURRENT_SOURCE_DIR}/lexer/IdentifierLexer.l
)

add_custom_command(
    DEPENDS reflex lexer/PPNumberLexer.l
    OUTPUT  "${GENERATED_DIR}/PPNumberLexer.yy.cpp" "${GENERATED_DIR}/PPNumberLexer.yy.h"
    COMMAND reflex "--header-file=${GENERATED_DIR}/PPNumberLexer.yy.h" "--outfile=${GENERATED_DIR}/PPNumberLexer.yy.cpp"
            ${CMAKE_CURRENT_SOURCE_DIR}/lexer/PPNumberLexer.l
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

add_subdirectory(command)
add_subdirectory(test)
add_subdirectory(third_party)
