add_executable(fieldTests
        FieldsTests.cpp)
target_link_libraries(fieldTests PRIVATE protolib::fields GTest::gtest_main GTest::gmock)

target_include_directories(fieldTests PRIVATE ./ ${PROJECT_SOURCE_DIR})

include(GoogleTest)
gtest_discover_tests(fieldTests)


