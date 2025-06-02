add_library(z80_cpu_dll SHARED z80_cpu.c)

set_target_properties(z80_cpu_dll
    PROPERTIES
        OUTPUT_NAME z80_cpu
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin"
)

# target_link_libraries(z80_cpu_dll
#     PRIVATE
#         utils
# )

target_include_directories(z80_cpu_dll
    PUBLIC
        ${PROJECT_SOURCE_DIR}/source
        ${PROJECT_SOURCE_DIR}/source/z80_cpu
        ${CMAKE_CURRENT_SOURCE_DIR}
)

set(CMAKE_C_FLAGS "")
set(CMAKE_CXX_FLAGS "")

target_compile_options(z80_cpu_dll
    PRIVATE
        -Wall
        -Wextra
        -lm
        -g
        -lws2_32
        -fstack-protector-all
)
