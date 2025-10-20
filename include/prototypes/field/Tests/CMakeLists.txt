add_executable(fieldTests
        TestFieldPrototypes.hpp
        TestFields.hpp
        FieldsTests.cpp)
target_link_libraries(fieldTests PRIVATE protolib::fields GTest::gtest_main GTest::gmock)

include(GoogleTest)
gtest_discover_tests(fieldTests)


