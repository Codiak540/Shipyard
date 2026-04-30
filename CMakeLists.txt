cmake_minimum_required(VERSION 4.3)
project(ship)

set(CMAKE_CXX_STANDARD 11)

add_executable(ship
        src/main.cpp
        src/port.cpp
        src/port.cpp
)

target_include_directories(ship PRIVATE
        include
)

find_package(CURL REQUIRED)

target_link_libraries(ship PRIVATE CURL::libcurl)