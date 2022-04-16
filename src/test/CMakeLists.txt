add_executable(m65dap_tests 
  connection_test.cpp
  m65_debugger_test.cpp
  memory_test.cpp
  mock_mega65.cpp
  mock_mega65.h
  test_common.cpp
  test_common.h
  trace_test.cpp
  util_test.cpp
  ../c64_debugger_data.cpp
  ../c64_debugger_data.h
  ../dap_logger.cpp
  ../dap_logger.h
  ../m65_debugger.cpp
  ../m65_debugger.h
  ../memory_cache.cpp
  ../memory_cache.h
  ../serial_connection.cpp
  ../serial_connection.h
  ../unix_connection.cpp
  ../unix_connection.h
  ../unix_domain_socket_connection.cpp
  ../unix_domain_socket_connection.h
  ../util.cpp
  ../util.h
)

find_package(GTest REQUIRED)
include(GoogleTest)

target_link_libraries(m65dap_tests PRIVATE GTest::GTest fmt::fmt tinyxml2::tinyxml2 cppdap)
target_include_directories(m65dap_tests PRIVATE ..)
target_precompile_headers(m65dap_tests PUBLIC ../pch.h)

gtest_discover_tests(m65dap_tests WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
