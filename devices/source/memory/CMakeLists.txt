add_library(memory_dll SHARED memory.c)

set_target_properties(memory_dll
    PROPERTIES
        OUTPUT_NAME memory
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin"
)

target_include_directories(memory_dll
    PUBLIC
        ${PROJECT_SOURCE_DIR}/source/memory
        ${CMAKE_CURRENT_SOURCE_DIR}
)

set(CMAKE_C_FLAGS "")
set(CMAKE_CXX_FLAGS "")

target_compile_options(memory_dll
    PRIVATE
        -Wall
        -Wextra
        -lm
        -g
        -lws2_32
        -fstack-protector-all
)
