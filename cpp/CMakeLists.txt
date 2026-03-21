cmake_minimum_required(VERSION 3.15)

# Project name and C++ standard
project(CPPEmulator VERSION 1.0 LANGUAGES CXX)

# Require C++17 or 20 for features like std::remove_if and structured bindings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_COMPILER C:\\mingw64\\bin\\g++.exe)   # 64-bit

# Enable CTest functionality
enable_testing()

# --- Executables ---

# 1. The Address Decoder Test
# Assuming your decoder and device logic are in headers or you'll add .cpp files here
add_executable(test_address_decoder test_address_decoder.cpp) 

# --- CTest Configuration ---

# Add the individual test to the CTest registry
add_test(NAME AddressDecoderTest COMMAND test_address_decoder)

# Create a custom target "test_all" that runs ctest
# This allows you to run 'ninja test_all'
add_custom_target(test_all
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_address_decoder
    COMMENT "Running all project tests..."
)