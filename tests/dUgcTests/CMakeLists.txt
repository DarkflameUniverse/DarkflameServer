add_executable(dUgcTests "UgcTests.cpp")
target_link_libraries(dUgcTests ${COMMON_LIBRARIES} dUgc GTest::gtest_main)
gtest_discover_tests(dUgcTests)
