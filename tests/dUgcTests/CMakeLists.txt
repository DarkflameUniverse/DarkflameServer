add_executable(dUgcTests "UgcTests.cpp")
target_include_directories(dUgcTests PRIVATE "${PROJECT_SOURCE_DIR}/dDatabase/GameDatabase/ITables")
target_link_libraries(dUgcTests ${COMMON_LIBRARIES} dUgc GTest::gtest_main)
gtest_discover_tests(dUgcTests)
target_compile_definitions(dUgcTests PRIVATE UGC_POSE_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/ugc-pose-fixture.json")

# The dashboard's pose editor math (static/js/ugc-pose-math.js) against the same fixture, when node is there
find_program(NODE_EXECUTABLE node)
if(NODE_EXECUTABLE)
	add_test(NAME UgcPoseMathJs COMMAND ${NODE_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/ugc-pose-math.test.mjs"
		"${PROJECT_SOURCE_DIR}/dDashboardServer/static/js/ugc-pose-math.js" "${CMAKE_CURRENT_SOURCE_DIR}/ugc-pose-fixture.json")
endif()
