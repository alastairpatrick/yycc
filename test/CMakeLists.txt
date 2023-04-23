include(FetchContent)

FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz)
FetchContent_MakeAvailable(json)

add_executable(test
    "run.cpp"
    "main.cpp"
 )

target_link_libraries(test PRIVATE yycc_lib nlohmann_json::nlohmann_json)
