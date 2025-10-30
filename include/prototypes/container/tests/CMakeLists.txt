add_executable(ContainerTests ContainerTests.cpp
        RxContainerTest.cpp
        TxContainerTest.cpp
        PingPongTest.cpp
)

target_link_libraries(ContainerTests PRIVATE protolib::containers GTest::gtest_main GTest::gmock)
target_include_directories(ContainerTests PRIVATE . ../../field/tests ${PROJECT_SOURCE_DIR})
include(GoogleTest)
gtest_discover_tests(ContainerTests)


