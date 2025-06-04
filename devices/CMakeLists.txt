cmake_minimum_required(VERSION 3.31)

SET(CMAKE_CACHE_DISABLE ON)
# set(CMAKE_C_COMPILER C:\\MinGW\\bin\\gcc.exe)   # 32-bit
set(CMAKE_C_COMPILER C:\\mingw64\\bin\\gcc.exe)   # 64-bit
# set(CMAKE_CXX_COMPILER /c/MinGW/bin/g++)
project(project_name)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # Generate compile_commands.json for vscode
include(CTest)

add_subdirectory(lib)
add_subdirectory(source)
add_subdirectory(tests)
