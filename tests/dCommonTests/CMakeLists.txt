set(DCOMMONTEST_SOURCES
	"AMFDeserializeTests.cpp"
	"ClientSysInfoTests.cpp"
	"Amf3Tests.cpp"
	"ToUnderlyingTests.cpp"
	"InstanceMigrationTests.cpp"
	"LiveUpdateTests.cpp"
	"TestCDFeatureGatingTable.cpp"
	"TestLDFFormat.cpp"
	"TestNiPoint3.cpp"
	"TestEncoding.cpp"
	"TestLUString.cpp"
	"TestLUWString.cpp"
	"TestBitStreamUtils.cpp"
	"MessageIdPinTests.cpp"
	"dCommonDependencies.cpp"
	"LxfmlTests.cpp"
	"MigrationSplitTests.cpp"
	"ConfigLayerTests.cpp"
	"SlashCommandLevelsTests.cpp"
	"AccountRulesTests.cpp"
	"VanityXmlTests.cpp"
	"EventPartsTests.cpp"
	"ScheduleRulesTests.cpp"
	"VanityEventsTests.cpp"
	"ZoneFileTests.cpp"
	"ClientZoneFilesTests.cpp"
	"ZoneScenesTests.cpp"
	"LiveOpsRulesTests.cpp"
	"ProcessAndLimitsTests.cpp"
	"HotPropertySlotsTests.cpp"
	"PropertyRentRulesTests.cpp"
	"PropertyReputationRulesTests.cpp"
	"BindAddressTests.cpp"
	"TrafficStatsTests.cpp"
	"ProfilerTests.cpp"
	"Sd0Tests.cpp"
	"FdbReaderTests.cpp"
	"FdbSnapshotTests.cpp"
	"WorldFileWatchTests.cpp"
)

add_subdirectory(dEnumsTests)
list(APPEND DCOMMONTEST_SOURCES ${DENUMS_TESTS})

# Set our executable
add_executable(dCommonTests ${DCOMMONTEST_SOURCES})

# Needs to be in binary dir for ctest 
if(APPLE)
	add_custom_target(dCommonTestsLink
		${CMAKE_COMMAND} -E copy $<TARGET_FILE:MariaDB::ConnCpp> ${CMAKE_CURRENT_BINARY_DIR})

	add_dependencies(dCommonTests dCommonTestsLink)
endif()

# Link needed libraries
target_link_libraries(dCommonTests ${COMMON_LIBRARIES} MD5 GTest::gtest_main)
# SpareBackoff.h (header only)
target_include_directories(dCommonTests PRIVATE "${PROJECT_SOURCE_DIR}/dMasterServer")

# Copy test files to testing directory
add_subdirectory(TestBitStreams)
file(COPY ${TESTBITSTREAMS} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
add_subdirectory(LxfmlTestFiles)
file(COPY ${LXFMLTESTFILES} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# VanityXmlTests reads the shipped vanity files
target_compile_definitions(dCommonTests PRIVATE DLU_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

# Discover the tests
gtest_discover_tests(dCommonTests)
