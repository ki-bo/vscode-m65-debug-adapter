set(target "m65dbg_adapter")
add_executable(${target}
    c64_debugger_data.cpp
    c64_debugger_data.h
    connection.h
    dap_logger.h
    dap_logger.cpp
    duration.h
    exception.h
    m65_dap_session.cpp
    m65_dap_session.h
    m65_debugger.cpp
    m65_debugger.h
    main.cpp
    memory_cache.cpp
    memory_cache.h
    serial_connection.h
    serial_connection.cpp
    unix_connection.cpp
    unix_connection.h
    unix_domain_socket_connection.cpp
    unix_domain_socket_connection.h
    util.cpp
    util.h
)

target_precompile_headers(${target}
PUBLIC
    pch.h    
)

find_package(fmt)
find_package(serial)
find_package(tinyxml2)

target_link_libraries(${target} 
    fmt::fmt
    cppdap
    serial::serial
    tinyxml2::tinyxml2
)
set(extension_output_dir "kibo.mega65-debugger-${PROJECT_VERSION}")
set_target_properties(${target} 
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/extension
        
)
if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    set(m65dbg_filename "${target}.exe")
else()
    set(m65dbg_filename "${target}")
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/package/package.json 
               ${CMAKE_BINARY_DIR}/extension/package.json
               @ONLY
)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../LICENSE 
               ${CMAKE_BINARY_DIR}/extension/LICENSE.txt
)

find_program(VSCE_PROGRAM vsce)
if(NOT VSCE_PROGRAM MATCHES "VSCE_PROGRAM-NOTFOUND")
    set(vsix_out "${CMAKE_BINARY_DIR}/kibo.mega65-debugger-${PROJECT_VERSION}.vsix")
    message("${vsix_out}")
    add_custom_command(
        OUTPUT "${vsix_out}"
        COMMAND ${VSCE_PROGRAM} package -o ${vsix_out}
        DEPENDS "${target}"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/extension"
    )
    add_custom_target(
        vsix-package
        ALL
        DEPENDS ${vsix_out}
    )
endif()
