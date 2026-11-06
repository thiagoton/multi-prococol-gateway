cmake_minimum_required(VERSION 3.16)

project(MultiProtocolGateway
    VERSION 0.1.0
    DESCRIPTION "Extensible multi-protocol gateway for IoT and industrial applications"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(GATEWAY_BUILD_APPS "Build gateway applications" ON)

add_subdirectory(src)

if(GATEWAY_BUILD_APPS)
    add_subdirectory(apps)
endif()
