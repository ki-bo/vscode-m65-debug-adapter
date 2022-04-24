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
    opcodes.cpp
    opcodes.h
    serial_connection.cpp
    serial_connection.h
    unix_connection.cpp
    unix_connection.h
    unix_domain_socket_connection.cpp
    unix_domain_socket_connection.h
    unix_serial_connection.h
    unix_serial_connection.cpp
    util.cpp
    util.h
)

target_precompile_headers(${target} PUBLIC pch.h)

find_package(fmt)
find_package(serial)
find_package(tinyxml2)

target_link_libraries(${target} 
    fmt::fmt
    cppdap
    serial::serial
    tinyxml2::tinyxml2
)
set_target_properties(${target} 
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/extension
        
)

add_subdirectory(test)
