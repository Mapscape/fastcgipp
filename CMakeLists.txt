cmake_minimum_required(VERSION 2.8)
project(fastcgi++ CXX)

set(VERSION "3.0alpha")
string(TIMESTAMP BUILD_TIME UTC)

# Set up our log level for fastcgi++/log.hpp
if(NOT LOG_LEVEL)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(LOG_LEVEL 1)
    elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(LOG_LEVEL 3)
    else()
        set(LOG_LEVEL 2)
    endif()
endif()

# We'll need this stuff for sockets stuff
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(SYSTEM "LINUX")
elseif(UNIX)
    set(SYSTEM "UNIX")
elseif(WIN32)
    set(SYSTEM "WINDOWS")
endif()

# Set compile flags for gcc and clang
if(UNIX)
    set(CMAKE_CXX_FLAGS "-O0 -std=c++14 -Wall -Werror")
    set(
        CMAKE_CXX_FLAGS_RELEASE
        "-O3 -march=native -mtune=native -std=c++14 -Wall -Werror")
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -ggdb -std=c++14 -Wall -Werror")
endif()

# Our configuration 
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/fastcgi++/config.h" @ONLY)

# Include directories
include_directories(include)
include_directories("${CMAKE_CURRENT_BINARY_DIR}/include")
install(DIRECTORY
    "${CMAKE_CURRENT_SOURCE_DIR}/include/fastcgi++"
    DESTINATION "include")

# The shared library itself
add_library(fastcgipp SHARED
    src/log.cpp
    src/http.cpp
    src/protocol.cpp
    src/sockets.cpp
    src/transceiver.cpp)
install(TARGETS fastcgipp LIBRARY DESTINATION lib)

# Install the header files of course
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/include/fastcgi++/config.h"
    DESTINATION "include/fastcgi++")

# All the test stuff
enable_testing()
add_executable(protocol_test
    EXCLUDE_FROM_ALL
    src/log.cpp
    src/protocol.cpp
    tests/protocol.cpp)
add_test("Fastcgipp::Protocol" protocol_test)
add_executable(http_test
    EXCLUDE_FROM_ALL
    src/log.cpp
    src/protocol.cpp
    src/http.cpp
    tests/http.cpp)
add_test("Fastcgipp::Http" http_test)
add_custom_target(
    tests DEPENDS
    protocol_test
    http_test)

# And finally the documentation
find_package(Doxygen)
if(DOXYGEN_FOUND)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/doc/Doxyfile.in"
        "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile" @ONLY)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/doc/doc.hpp.in"
        "${CMAKE_CURRENT_BINARY_DIR}/doc.hpp" @ONLY)
    add_custom_target(
        doc
        "${DOXYGEN_EXECUTABLE}" "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        COMMENT "Generating API documentation with Doxygen" VERBATIM)
    install(
        DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/doc"
        DESTINATION "share/fastcgi++")
endif(DOXYGEN_FOUND)
