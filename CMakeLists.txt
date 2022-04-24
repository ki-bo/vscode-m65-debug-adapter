cmake_minimum_required(VERSION 3.18)

project(vscode-m65-debug-adapter 
        LANGUAGES CXX 
        VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)

include(cmake/conan_setup.cmake)
enable_testing()

add_subdirectory(3rd)
add_subdirectory(src)

set(extension_basename "kibo.mega65-debugger-${PROJECT_VERSION}")
set(m65dbg_filename "m65dbg_adapter")
if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    string(APPEND m65dbg_filename ".exe")
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/package.json 
               ${CMAKE_BINARY_DIR}/extension/package.json
               @ONLY
)

add_custom_target(extension_files 
    ALL
    COMMENT "Copying files: ${extension_input_files}"
)

add_custom_command(
  TARGET extension_files
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE ${CMAKE_BINARY_DIR}/extension/LICENSE.txt
)

set(extension_input_files
  images/icon.png
)

foreach(FILENAME ${extension_input_files})
    set(SRC "${CMAKE_CURRENT_SOURCE_DIR}/${FILENAME}")
    set(DST "${CMAKE_BINARY_DIR}/extension/${FILENAME}")

    add_custom_command(
        TARGET extension_files
        COMMAND ${CMAKE_COMMAND} -E copy ${SRC} ${DST}
    )
endforeach(FILENAME)

find_program(VSCE_PROGRAM vsce)
if(NOT VSCE_PROGRAM MATCHES "VSCE_PROGRAM-NOTFOUND")
    set(vsix_out "${CMAKE_BINARY_DIR}/${extension_basename}.vsix")
    add_custom_command(
        OUTPUT "${vsix_out}"
        COMMAND ${VSCE_PROGRAM} package -o ${vsix_out}
        DEPENDS m65dbg_adapter extension_files ${CMAKE_BINARY_DIR}/extension/package.json
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/extension"
    )
    add_custom_target(
        vsix-package
        ALL
        DEPENDS ${vsix_out}
    )
endif()
