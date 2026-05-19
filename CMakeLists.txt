cmake_minimum_required(VERSION 3.13)

# Pull in SDK functionality of pico-sdk
include(pico_sdk_import.cmake)

project(PicoCZ C CXX ASM)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)

# Initialize the Pico SDK
pico_sdk_init()

# Add the source directory
add_subdirectory(src)