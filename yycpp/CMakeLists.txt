add_executable(yycpp
    Sweep.cpp
    main.cpp
)

target_link_libraries(yycpp PRIVATE lang_lib reflex_lib)
