add_executable(dUgcTests "UgcTests.cpp")
target_include_directories(dUgcTests PRIVATE "${PROJECT_SOURCE_DIR}/dDatabase/GameDatabase/ITables")
target_link_libraries(dUgcTests ${COMMON_LIBRARIES} dUgc GTest::gtest_main)
gtest_discover_tests(dUgcTests)
