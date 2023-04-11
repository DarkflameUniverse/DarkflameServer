SET(DGAMEMESSAGES_TESTS
	"GameMessageTests.cpp")

# Get the folder name and prepend it to the files above
get_filename_component(thisFolderName ${CMAKE_CURRENT_SOURCE_DIR} NAME)
list(TRANSFORM DGAMEMESSAGES_TESTS PREPEND "${thisFolderName}/")

# Copy test files to testing directory
add_subdirectory(TestBitStreams)
list(TRANSFORM GAMEMESSAGE_TESTBITSTREAMS PREPEND "${thisFolderName}/")
set(GAMEMESSAGE_TESTBITSTREAMS ${GAMEMESSAGE_TESTBITSTREAMS} PARENT_SCOPE)

# Export to parent scope
set(DGAMEMESSAGES_TESTS ${DGAMEMESSAGES_TESTS} PARENT_SCOPE)
