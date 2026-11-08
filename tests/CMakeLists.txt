find_package(GTest QUIET)

if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    FetchContent_MakeAvailable(googletest)
endif()

if(TARGET GTest::gtest_main)
    set(GATEWAY_GTEST_MAIN_TARGET GTest::gtest_main)
elseif(TARGET GTest::Main)
    set(GATEWAY_GTEST_MAIN_TARGET GTest::Main)
else()
    set(GATEWAY_GTEST_MAIN_TARGET gtest_main)
endif()

add_executable(gateway_tests
    test_can.cpp
    test_serial.cpp
    test_socket.cpp)

target_include_directories(gateway_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(gateway_tests
    PRIVATE
        gateway::gateway
        ${GATEWAY_GTEST_MAIN_TARGET})

include(GoogleTest)
gtest_discover_tests(gateway_tests PROPERTIES TIMEOUT 20)
