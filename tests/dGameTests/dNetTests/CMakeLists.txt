SET(DNET_TESTS
	"ChatPacketsTests.cpp"
	"GuildPacketsTests.cpp"
	"CommonAuthPacketsTests.cpp"
	"MasterPacketsTests.cpp"
	"ServerTrafficTests.cpp"
	"WorldPacketsTests.cpp")

# Get the folder name and prepend it to the files above
get_filename_component(thisFolderName ${CMAKE_CURRENT_SOURCE_DIR} NAME)
list(TRANSFORM DNET_TESTS PREPEND "${thisFolderName}/")

# Export to parent scope
set(DNET_TESTS ${DNET_TESTS} PARENT_SCOPE)
