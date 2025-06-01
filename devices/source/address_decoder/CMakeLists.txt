add_library(address_decoder_dll SHARED address_decoder.c)

set_target_properties(address_decoder_dll
    PROPERTIES
        OUTPUT_NAME address_decoder
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin"
)

target_include_directories(address_decoder_dll
    PUBLIC
        ${PROJECT_SOURCE_DIR}/source
        ${PROJECT_SOURCE_DIR}/source/address_decoder
        ${CMAKE_CURRENT_SOURCE_DIR}
)

set(CMAKE_C_FLAGS "")
set(CMAKE_CXX_FLAGS "")

target_compile_options(address_decoder_dll
    PRIVATE
        -Wall
        -Wextra
        -lm
        -g
        -lws2_32
        -fstack-protector-all
)
