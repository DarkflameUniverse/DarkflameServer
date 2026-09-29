set(DCOMPONENTS_TESTS
	"DeletionRestrictionTests.cpp"
	"InventorySaveTests.cpp"
	"EquipMessagesTests.cpp"
	"DestroyableLevelConfigTests.cpp"
	"RailActivatorComponentTests.cpp"
	"VendorBuybackTests.cpp"
	"RacingWrongWayTests.cpp"
	"DestroyableComponentTests.cpp"
	"PetComponentTests.cpp"
	"SimplePhysicsComponentTests.cpp"
	"SavingTests.cpp"
	"CharacterSaveFieldsTests.cpp"
	"ReplicaComponentOrderTests.cpp"
)

# Get the folder name and prepend it to the files above
get_filename_component(thisFolderName ${CMAKE_CURRENT_SOURCE_DIR} NAME)
list(TRANSFORM DCOMPONENTS_TESTS PREPEND "${thisFolderName}/")

# Export to parent scope
set(DCOMPONENTS_TESTS ${DCOMPONENTS_TESTS} PARENT_SCOPE)

# Copy test files to testing directory
add_subdirectory(TestData)
list(TRANSFORM COMPONENT_TEST_DATA PREPEND "${thisFolderName}/")
set(COMPONENT_TEST_DATA ${COMPONENT_TEST_DATA} PARENT_SCOPE)
