add_executable(exoAtlantProtocolTest exoAtlantProtocolTest.cpp)

target_link_libraries(exoAtlantProtocolTest PRIVATE GTest::gtest_main exoAtlantProtocol protolib::interfaces)
include(GoogleTest)
gtest_discover_tests(exoAtlantProtocolTest)


