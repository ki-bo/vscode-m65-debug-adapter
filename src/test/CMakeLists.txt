add_executable(tests 
  connection_test.cpp
  memory_test.cpp
  mock_mega65.cpp
  mock_mega65.h
  util_test.cpp
)

find_package(GTest REQUIRED)
include(GoogleTest)

target_link_libraries(tests PRIVATE GTest::GTest fmt::fmt)
target_include_directories(tests PRIVATE ..)
target_precompile_headers(tests
PUBLIC
    ../pch.h    
)

gtest_discover_tests(tests)
