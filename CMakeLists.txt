cmake_minimum_required(VERSION 3.21)

project(lang)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")

add_library(lang_lib STATIC
    assoc_prec.c
    assoc_prec.h
    ast.cpp
    ast.h
    decl.cpp
    decl.h
    expr.cpp
    expr.h
    print.cpp
    print.h
    stmt.cpp
    stmt.h
    type.cpp
    type.h
    ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.h
)

add_custom_command(
    DEPENDS reflex lex.l
    OUTPUT  lex.yy.cpp lex.yy.h
    COMMAND reflex --header-file
            -o ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/lex.l
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