set(DENUMS_TESTS
	"MacroStringifyTests.cpp"
)

# Get the folder name and prepend it to the files above
get_filename_component(thisFolderName ${CMAKE_CURRENT_SOURCE_DIR} NAME)
list(TRANSFORM DENUMS_TESTS PREPEND "${thisFolderName}/")

# Export to parent scope
set(DENUMS_TESTS ${DENUMS_TESTS} PARENT_SCOPE)
