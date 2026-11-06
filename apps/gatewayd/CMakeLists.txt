add_executable(gatewayd main.cpp)

target_link_libraries(gatewayd
    PRIVATE
        gateway::gateway)
