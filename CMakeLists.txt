cmake_minimum_required(VERSION 3.20.6)

project(lang)

set(CMAKE_CXX_STANDARD 20)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")

set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

add_library(yycc_lib_c STATIC
    "parse/AssocPrec.c"
)

add_library(yycc_lib STATIC
    "FileCache.cpp"
    "InternedString.cpp"
    "LLVM.cpp"
    "Message.cpp"
    "Printable.cpp"
    "TranslationUnitContext.cpp"
    "Value.cpp"

    "lex/Identifier.cpp"
    "lex/Location.cpp"
    "lex/StringLiteral.cpp"

    "parse/ASTNode.cpp"
    "parse/Constant.cpp"
    "parse/Declaration.cpp"
    "parse/Expr.cpp"
    "parse/IdentifierMap.cpp"
    "parse/Parser.cpp"
    "parse/Statement.cpp"
    "parse/ArrayType.cpp"
    "parse/Type.cpp"
    "parse/TypeContext.cpp"

    "preprocess/Preprocessor.cpp"
    
    "visit/Emitter.cpp"
    "visit/EntityPass.cpp"
    "visit/ResolvePass.cpp"
    "visit/Visitor.cpp"

    "Sweep.cpp"
    "TextStream.cpp"

    "${GENERATED_DIR}/PPNumberLexer.yy.cpp"
    "${GENERATED_DIR}/PPTokenLexer.yy.cpp"
    "${GENERATED_DIR}/IdentifierLexer.yy.cpp"
)

target_precompile_headers(yycc_lib PUBLIC "pch.h")

add_custom_command(
    DEPENDS reflex lex/PPTokenLexer.l
    OUTPUT  "${GENERATED_DIR}/PPTokenLexer.yy.cpp" "${GENERATED_DIR}/PPTokenLexer.yy.h"
    COMMAND reflex "--header-file=${GENERATED_DIR}/PPTokenLexer.yy.h" "--outfile=${GENERATED_DIR}/PPTokenLexer.yy.cpp"
            ${CMAKE_CURRENT_SOURCE_DIR}/lex/PPTokenLexer.l
)

add_custom_command(
    DEPENDS reflex lex/IdentifierLexer.l
    OUTPUT  "${GENERATED_DIR}/IdentifierLexer.yy.cpp" "${GENERATED_DIR}/IdentifierLexer.yy.h"
    COMMAND reflex "--header-file=${GENERATED_DIR}/IdentifierLexer.yy.h" "--outfile=${GENERATED_DIR}/IdentifierLexer.yy.cpp"
            ${CMAKE_CURRENT_SOURCE_DIR}/lex/IdentifierLexer.l
)

add_custom_command(
    DEPENDS reflex lex/PPNumberLexer.l
    OUTPUT  "${GENERATED_DIR}/PPNumberLexer.yy.cpp" "${GENERATED_DIR}/PPNumberLexer.yy.h"
    COMMAND reflex "--header-file=${GENERATED_DIR}/PPNumberLexer.yy.h" "--outfile=${GENERATED_DIR}/PPNumberLexer.yy.cpp"
            ${CMAKE_CURRENT_SOURCE_DIR}/lex/PPNumberLexer.l
)

target_include_directories(yycc_lib
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}
        third_party
        third_party/reflex/include
        .
        ${LLVM_INCLUDE_DIRS}
)

target_compile_definitions(yycc_lib PRIVATE
    ${LLVM_DEFINITIONS}
)

llvm_map_components_to_libnames(lang_llvm_libs core executionengine mcjit interpreter analysis native bitwriter ${LLVM_ALL_TARGETS})

target_link_libraries(yycc_lib PRIVATE
    yycc_lib_c
    reflex_lib
    nlohmann_json::nlohmann_json
    ${lang_llvm_libs}
)

add_subdirectory(command)
add_subdirectory(test)
add_subdirectory(third_party)
