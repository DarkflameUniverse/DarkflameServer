SET(DZONEMANAGER_TESTS
	"RawTests.cpp"
	"ZoneSceneTests.cpp")

# Get the folder name and prepend it to the files above
get_filename_component(thisFolderName ${CMAKE_CURRENT_SOURCE_DIR} NAME)
list(TRANSFORM DZONEMANAGER_TESTS PREPEND "${thisFolderName}/")

# Export to parent scope
set(DZONEMANAGER_TESTS ${DZONEMANAGER_TESTS} PARENT_SCOPE)
