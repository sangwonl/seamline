cmake_minimum_required(VERSION 3.16)
project(seamline VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SEAMLINE_BUILD_TESTS "Build test suite" ON)
option(SEAMLINE_BUILD_SHARED "Build shared library" OFF)
option(SEAMLINE_BUILD_CLI "Build CLI tool" OFF)

include(cmake/Dependencies.cmake)

set(SEAMLINE_SOURCES
    src/image.cpp
    src/stitch.cpp
    src/seamline.cpp
    src/stb_impl.cpp
)

if(SEAMLINE_BUILD_SHARED)
    add_library(seamline SHARED ${SEAMLINE_SOURCES})
else()
    add_library(seamline STATIC ${SEAMLINE_SOURCES})
endif()

target_include_directories(seamline
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/deps/stb
)

if(SEAMLINE_BUILD_CLI)
    add_executable(stitch_cli tools/stitch_cli.cpp)
    target_link_libraries(stitch_cli PRIVATE seamline)
endif()

if(SEAMLINE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
