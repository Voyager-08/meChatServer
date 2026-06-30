cmake_minimum_required(VERSION 3.16)
project(meChatServer)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 REQUIRED COMPONENTS Core Network Sql)

add_executable(meChatServer
    src/main.cpp
    src/chatserver.cpp
    src/chatserver.h
    src/database.cpp
    src/database.h
)

target_link_libraries(meChatServer Qt5::Core Qt5::Network Qt5::Sql)