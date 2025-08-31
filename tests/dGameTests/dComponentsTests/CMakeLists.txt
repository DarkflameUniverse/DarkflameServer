set(DCOMPONENTS_TESTS
	"ActivityComponentTests.cpp"
	"BaseCombatAIComponentTests.cpp"
	"BouncerComponentTests.cpp"
	"BuffComponentTests.cpp"
	"CollectibleComponentTests.cpp"
	"ControllablePhysicsComponentTests.cpp"
	"DestroyableComponentTests.cpp"
	"DonationVendorComponentTests.cpp"
	"HavokVehiclePhysicsComponentTests.cpp"
	"ItemComponentTests.cpp"
	"LevelProgressionComponentTests.cpp"
	"LUPExhibitComponentTests.cpp"
	"MiniGameControlComponentTests.cpp"
	"ModelComponentTests.cpp"
	"MovingPlatformComponentTests.cpp"
	"PetComponentTests.cpp"
	"QuickBuildComponentTests.cpp"
	"RenderComponentTests.cpp"
	"SavingTests.cpp"
	"SimplePhysicsComponentTests.cpp"
	"SwitchComponentTests.cpp"
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
