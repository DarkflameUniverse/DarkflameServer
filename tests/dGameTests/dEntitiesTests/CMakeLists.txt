set(DENTITIES_TESTS
	"ArchetypeTests.cpp"
)

# Get the folder name and prepend it to the files above
get_filename_component(thisFolderName ${CMAKE_CURRENT_SOURCE_DIR} NAME)
list(TRANSFORM DENTITIES_TESTS PREPEND "${thisFolderName}/")

# Export to parent scope
set(DENTITIES_TESTS ${DENTITIES_TESTS} PARENT_SCOPE)
