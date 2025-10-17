cmake_minimum_required(VERSION 3.12)
project(cpp_proxy LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)

add_executable(cpp_proxy
  src/main.cpp
  src/proxy_server.cpp
  src/connection.cpp
  src/http_client.cpp
)

target_include_directories(cpp_proxy PRIVATE src)

if (WIN32)
  target_compile_definitions(cpp_proxy PRIVATE _WIN32_WINNT=0x0601)
  target_link_libraries(cpp_proxy PRIVATE ws2_32)
endif()
