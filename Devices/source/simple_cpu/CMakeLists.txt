
add_library(simple_cpu SHARED simple_cpu.c)

set_target_properties(simple_cpu PROPERTIES OUTPUT_NAME simple_cpu)

set_target_properties(simple_cpu PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

target_link_libraries(simple_cpu
    PRIVATE
        utils
)

target_include_directories(simple_cpu
    PUBLIC
        ${PROJECT_SOURCE_DIR}/source/simple_cpu
        ${CMAKE_CURRENT_SOURCE_DIR}
)

set(CMAKE_C_FLAGS "")
set(CMAKE_CXX_FLAGS "")

target_compile_options(simple_cpu
    PRIVATE
        -Wall
        -Wextra
        -lm
        -g
        -lws2_32
        -fstack-protector-all
)
