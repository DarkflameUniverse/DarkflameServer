set(DGAME_DINVENTORY_SOURCES
	"EquippedItem.cpp"
	"Inventory.cpp"
	"Item.cpp"
	"ItemSet.cpp"
	"ItemSetPassiveAbility.cpp")

# Workaround for compiler bug where the optimized code could result in a memcpy of 0 bytes, even though that isnt possible.
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=97185
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	set_source_files_properties("Item.cpp" PROPERTIES COMPILE_FLAGS "-Wno-stringop-overflow")
endif()

add_library(dInventory STATIC ${DGAME_DINVENTORY_SOURCES})
target_precompile_headers(dInventory REUSE_FROM dGameBase)
