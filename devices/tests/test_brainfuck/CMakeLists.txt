add_executable(test_brainfuck_cpu test_brainfuck_cpu.c)

target_link_libraries(test_brainfuck_cpu
    PRIVATE
        brainfuck_cpu_lib
)

target_include_directories(test_brainfuck_cpu
    PUBLIC
        ${PROJECT_SOURCE_DIR}/source
        ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(NAME test_brainfuck_cpu COMMAND test_brainfuck_cpu)
