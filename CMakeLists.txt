cmake_minimum_required(VERSION 3.18)

project(vscode-m65-debug-adapter 
        LANGUAGES CXX 
        VERSION 0.1.0)

        set(CMAKE_CXX_STANDARD 20)

include(cmake/conan_setup.cmake)

add_subdirectory(3rd)
add_subdirectory(src)
