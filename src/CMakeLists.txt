add_library(gateway)
add_library(gateway::gateway ALIAS gateway)

target_include_directories(gateway
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(gateway
        PRIVATE
            transport/linux/can.cpp
            transport/linux/serial.cpp
            transport/linux/socket.cpp)
else()
    message(FATAL_ERROR "No transport implementation is available for ${CMAKE_SYSTEM_NAME}")
endif()
