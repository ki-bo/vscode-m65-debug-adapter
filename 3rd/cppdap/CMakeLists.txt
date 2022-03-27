add_library(cppdap STATIC 
  src/content_stream.cpp
  src/io.cpp
  src/nlohmann_json_serializer.cpp
  src/null_json_serializer.cpp
  src/protocol_events.cpp
  src/protocol_requests.cpp
  src/protocol_response.cpp
  src/protocol_types.cpp
  src/session.cpp
  src/socket.cpp
  src/typeinfo.cpp
  src/typeof.cpp
)

find_package(nlohmann_json)

target_compile_definitions(cppdap PRIVATE CPPDAP_JSON_NLOHMANN)
target_include_directories(cppdap PUBLIC include)
target_link_libraries(cppdap PRIVATE nlohmann_json::nlohmann_json)
