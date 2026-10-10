cmake_minimum_required(VERSION 3.20)
project(MusicPadDaemon CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# nlohmann_json
find_package(nlohmann_json QUIET)
if (NOT nlohmann_json_FOUND)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# IXWebSocket
set(USE_TLS OFF CACHE BOOL "Disable TLS for local websocket" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static" FORCE)
FetchContent_Declare(
    ixwebsocket
    GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
    GIT_TAG v11.4.5
)
FetchContent_MakeAvailable(ixwebsocket)

set(DAEMON_SOURCES
    src/main.cpp
    src/backend/MockMediaBackend.cpp
    src/server/WebSocketServer.cpp
)

set(PLATFORM_LIBS "")

if (WIN32)
    list(APPEND DAEMON_SOURCES src/backend/WindowsMediaBackend.cpp)
    list(APPEND PLATFORM_LIBS
        windowsapp
        ole32
        oleaut32
        iphlpapi
        ws2_32
        gdiplus
    )
elseif (UNIX AND NOT APPLE)
    list(APPEND DAEMON_SOURCES src/backend/LinuxMediaBackend.cpp)
    list(APPEND PLATFORM_LIBS
        sdbus-c++
        pthread
    )
endif()

add_executable(musicpad_daemon ${DAEMON_SOURCES})

target_include_directories(musicpad_daemon PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(musicpad_daemon PRIVATE
    ixwebsocket
    nlohmann_json::nlohmann_json
    ${PLATFORM_LIBS}
)

if (MSVC)
    target_compile_options(musicpad_daemon PRIVATE /W4 /permissive-)
else()
    target_compile_options(musicpad_daemon PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

message(STATUS "MusicPad daemon configured successfully")
