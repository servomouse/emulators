add_library(brainfuck_cpu_dll SHARED brainfuck_cpu.c)

set_target_properties(brainfuck_cpu_dll
    PROPERTIES
        OUTPUT_NAME brainfuck_cpu
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin"
)

target_link_libraries(brainfuck_cpu_dll
    PRIVATE
        utils
)

target_include_directories(brainfuck_cpu_dll
    PUBLIC
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/source
        ${PROJECT_SOURCE_DIR}/source/brainfuck_cpu
        ${CMAKE_CURRENT_SOURCE_DIR}
)

set(CMAKE_C_FLAGS "")
set(CMAKE_CXX_FLAGS "")

target_compile_options(brainfuck_cpu_dll
    PRIVATE
        -Wall
        -Wextra
        -lm
        -g
        -lws2_32
        -fstack-protector-all
)
