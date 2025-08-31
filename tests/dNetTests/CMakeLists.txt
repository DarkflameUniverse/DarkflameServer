set(DNETTESTS_SOURCES
	"TestCommon.cpp"
	"AuthPacketTests.cpp"
	"ChatPacketTests.cpp"
	"WorldPacketTests.cpp"
	"MasterPacketTests.cpp"
	"PacketEdgeCaseTests.cpp"
	"MailBitStreamTests.cpp"
)

# Set our executable  
add_executable(dNetTests ${DNETTESTS_SOURCES})

# Needs to be in binary dir for ctest
if(APPLE)
	add_custom_target(dNetTestsLink
		${CMAKE_COMMAND} -E copy $<TARGET_FILE:MariaDB::ConnCpp> ${CMAKE_CURRENT_BINARY_DIR})

	add_dependencies(dNetTests dNetTestsLink)
endif()

# Link needed libraries
target_link_libraries(dNetTests ${COMMON_LIBRARIES} GTest::gtest_main dNet)

# Discover the tests
gtest_discover_tests(dNetTests)