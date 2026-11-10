set(DCOMPONENTS_TESTS
	"DeletionRestrictionTests.cpp"
	"MovingPlatformComponentTests.cpp"
	"InventorySaveTests.cpp"
	"EquipMessagesTests.cpp"
	"DestroyableLevelConfigTests.cpp"
	"ProximityMonitorComponentTests.cpp"
	"RailActivatorComponentTests.cpp"
	"ConstructionOnlyComponentTests.cpp"
	"PropertyVendorComponentTests.cpp"
	"VendorBuybackTests.cpp"
	"RacingWrongWayTests.cpp"
	"DestroyableComponentTests.cpp"
	"InventoryGroupTests.cpp"
	"ModelComponentTests.cpp"
	"PetComponentTests.cpp"
	"SimplePhysicsComponentTests.cpp"
	"MovementBlockerScenarioTests.cpp"
	"SavingTests.cpp"
	"CharacterSaveFieldsTests.cpp"
	"CollectibleComponentTests.cpp"
	"MissionOfferTests.cpp"
	"MissionClientRulesTests.cpp"
	"MissionRewardSpaceTests.cpp"
	"ReplicaComponentOrderTests.cpp"
	"QuickBuildCompleteTests.cpp"
	"RocketLaunchTests.cpp"
	"ReplicaConstructionTests.cpp"
	"ReplicaDecoderTests.cpp"
	"CapturePropertyTests.cpp"
	"InventoryGroupTests.cpp"
	"InventoryMoveTests.cpp"
	"ItemRemovalTests.cpp"
	"UseItemResultTests.cpp"
	"LootMetricsTests.cpp"
	"SpawnerComponentTests.cpp"
	"SkillComponentTests.cpp"
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
