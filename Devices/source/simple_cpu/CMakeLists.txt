# set(CMAKE_RUNTIME_OUTPUT_DIRECTORY  ${PROJECT_SOURCE_DIR}/bin/)

add_library(simple_cpu_dll SHARED simple_cpu.c)

set_target_properties(simple_cpu_dll
    PROPERTIES
        OUTPUT_NAME simple_cpu
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin"
)

# set_target_properties(simple_cpu_dll PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")

# target_link_libraries(simple_cpu_dll
#     PRIVATE
#         utils
# )

target_include_directories(simple_cpu_dll
    PUBLIC
        ${PROJECT_SOURCE_DIR}/source/simple_cpu_dll
        ${CMAKE_CURRENT_SOURCE_DIR}
)

set(CMAKE_C_FLAGS "")
set(CMAKE_CXX_FLAGS "")

target_compile_options(simple_cpu_dll
    PRIVATE
        -Wall
        -Wextra
        -lm
        -g
        -lws2_32
        -fstack-protector-all
)

add_executable(simple_cpu simple_cpu.c)

set_target_properties(simple_cpu PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")

# target_link_libraries(simple_cpu
#     PRIVATE
#         utils
# )

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
