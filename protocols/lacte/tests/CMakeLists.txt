add_executable(lacteProtocolTest LacteProtocolTest.cpp)

target_link_libraries(lacteProtocolTest PRIVATE GTest::gtest_main lacte_protocol)
target_include_directories(lacteProtocolTest PRIVATE ../)
include(GoogleTest)
gtest_discover_tests(lacteProtocolTest)


