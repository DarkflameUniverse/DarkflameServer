add_executable(dECSTests
    "TestECS.cpp"
)

# Link needed libraries
target_link_libraries(dECSTests PRIVATE dCommon dGame dECS GTest::gtest_main)

# Discover the tests
gtest_discover_tests(dECSTests)
