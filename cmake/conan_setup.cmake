list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})

if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
  message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
  file(DOWNLOAD "https://raw.githubusercontent.com/conan-io/cmake-conan/release/0.17/conan.cmake"
                 "${CMAKE_BINARY_DIR}/conan.cmake"
       EXPECTED_HASH SHA256=3bef79da16c2e031dc429e1dac87a08b9226418b300ce004cc125a82687baeef
       TLS_VERIFY ON)
endif()

include(${CMAKE_BINARY_DIR}/conan.cmake)

conan_cmake_configure(REQUIRES 
                        fmt/8.1.1 
                        nlohmann_json/3.10.5
                        serial/1.2.1
                      GENERATORS 
                        cmake_find_package)

conan_cmake_autodetect(settings)
conan_cmake_install(PATH_OR_REFERENCE .
                    BUILD missing
                    REMOTE conancenter
                    SETTINGS ${settings})
