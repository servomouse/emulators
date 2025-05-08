add_library(utils
    utils.c
    work_with_folders.c
)

target_include_directories(utils
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
