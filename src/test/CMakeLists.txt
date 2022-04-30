set(debugger_sources
  ../c64_debugger_data.cpp
  ../c64_debugger_data.h
  ../logger.cpp
  ../logger.h
  ../m65_debugger.cpp
  ../m65_debugger.h
  ../memory_cache.cpp
  ../memory_cache.h
  ../opcodes.h
  ../serial_connection.cpp
  ../serial_connection.h
  ../unix_connection.cpp
  ../unix_connection.h
  ../unix_domain_socket_connection.cpp
  ../unix_domain_socket_connection.h
  ../unix_serial_connection.cpp
  ../unix_serial_connection.h
  ../util.cpp
  ../util.h
)

set(debugger_libs
  fmt::fmt serial::serial tinyxml2::tinyxml2
)

add_executable(m65dap_tests 
  ${debugger_sources}
  connection_test.cpp
  expressions_test.cpp
  m65_debugger_test.cpp
  memory_test.cpp
  mock_mega65.cpp
  mock_mega65.h
  mock_mega65_fixture.h
  opcode_test.cpp
  test_common.cpp
  test_common.h
  trace_test.cpp
  util_test.cpp
)

find_package(GTest REQUIRED)
include(GoogleTest)

target_link_libraries(m65dap_tests PRIVATE GTest::GTest fmt::fmt serial::serial tinyxml2::tinyxml2)
target_include_directories(m65dap_tests PRIVATE ..)
target_precompile_headers(m65dap_tests PUBLIC ../pch.h)

gtest_discover_tests(m65dap_tests WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})


add_executable(compare_hw_and_mock
  ${debugger_sources}
  compare_hw_and_mock_main.cpp
  connection_proxy.cpp
  connection_proxy.h
  mock_mega65.cpp
  mock_mega65.h
)

target_link_libraries(compare_hw_and_mock PRIVATE ${debugger_libs})
target_include_directories(compare_hw_and_mock PRIVATE ..)
target_precompile_headers(compare_hw_and_mock PUBLIC ../pch.h)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/data/test.dbg.in 
               ${CMAKE_CURRENT_SOURCE_DIR}/data/test.dbg
               @ONLY
)
