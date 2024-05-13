set(DSCRIPTS_TESTS
	"dScriptsTests.cpp"
	"CppScriptsOld.cpp"
)

# Get the folder name and prepend it to the files above
get_filename_component(thisFolderName ${CMAKE_CURRENT_SOURCE_DIR} NAME)
list(TRANSFORM DSCRIPTS_TESTS PREPEND "${thisFolderName}/")

# Export to parent scope
set(DSCRIPTS_TESTS ${DSCRIPTS_TESTS} PARENT_SCOPE)
