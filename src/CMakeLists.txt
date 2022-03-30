add_executable(m65dbg_adapter
    connection.h
    dap_logger.h
    dap_logger.cpp
    duration.h
    m65_dap_session.cpp
    m65_dap_session.h
    m65_debugger.cpp
    m65_debugger.h
    main.cpp
    serial_connection.h
    serial_connection.cpp
)

target_precompile_headers(m65dbg_adapter
PUBLIC
    pch.h    
)

find_package(fmt)
find_package(serial)

target_link_libraries(m65dbg_adapter 
    fmt::fmt
    cppdap
    serial::serial
)
