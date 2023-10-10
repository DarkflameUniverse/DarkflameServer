set(DCOMPONENTS_TESTS
	"DestroyableComponentTests.cpp"
	"SimplePhysicsComponentTests.cpp"
)

# Get the folder name and prepend it to the files above
get_filename_component(thisFolderName ${CMAKE_CURRENT_SOURCE_DIR} NAME)
list(TRANSFORM DCOMPONENTS_TESTS PREPEND "${thisFolderName}/")

# Export to parent scope
set(DCOMPONENTS_TESTS ${DCOMPONENTS_TESTS} PARENT_SCOPE)
