set(TEST_BRAINFUCK_CPU_SOURCES
    test_brainfuck_cpu.c
    ${PROJECT_SOURCE_DIR}/source/brainfuck_cpu/brainfuck_cpu.c
)

add_executable(test_brainfuck_cpu ${TEST_BRAINFUCK_CPU_SOURCES})

target_include_directories(test_brainfuck_cpu
    PUBLIC
        ${PROJECT_SOURCE_DIR}/source
        ${CMAKE_CURRENT_SOURCE_DIR}
)

set(CMAKE_C_FLAGS "")
set(CMAKE_CXX_FLAGS "")

target_compile_options(test_brainfuck_cpu
    PRIVATE
        -Wall
        -Wextra
        -lm
        -g
        # -lws2_32
        # -fstack-protector-all
        # -m32
)

add_test(NAME test_brainfuck_cpu COMMAND test_brainfuck_cpu)
