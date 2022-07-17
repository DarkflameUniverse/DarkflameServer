# create the testing file and list of tests
create_test_sourcelist (Tests
	CommonCxxTests.cpp
	TestNiPoint3.cpp
	TestLDFFormat.cpp
)

# add the executable
add_executable (CommonCxxTests ${Tests})
target_link_libraries(CommonCxxTests ${COMMON_LIBRARIES})

# remove the test driver source file
set (TestsToRun ${Tests})
remove (TestsToRun CommonCxxTests.cpp)

# Add all the ADD_TEST for each test
foreach (test ${TestsToRun})
	get_filename_component (TName ${test} NAME_WE)
	add_test (NAME ${TName} COMMAND CommonCxxTests ${TName})
	set_property(TEST ${TName} PROPERTY ENVIRONMENT CTEST_OUTPUT_ON_FAILURE=1)
endforeach ()
