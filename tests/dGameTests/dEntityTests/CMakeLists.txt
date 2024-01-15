set(DENTITY_TESTS
	"EntityTests.cpp"
)

# Get the folder name and prepend it to the files above
get_filename_component(thisFolderName ${CMAKE_CURRENT_SOURCE_DIR} NAME)
list(TRANSFORM DENTITY_TESTS PREPEND "${thisFolderName}/")

# Export to parent scope
set(DENTITY_TESTS ${DENTITY_TESTS} PARENT_SCOPE)
