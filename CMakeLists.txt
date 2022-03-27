cmake_minimum_required(VERSION 3.16)

project(vscode-m65-debug-adapter LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)

include(cmake/conan_setup.cmake)

add_subdirectory(3rd)
add_subdirectory(src)
