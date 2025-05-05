add_subdirectory(CDClientDatabase)
add_subdirectory(GameDatabase)

add_library(dDatabase STATIC "MigrationRunner.cpp" "ModelNormalizeMigration.cpp")

add_custom_target(conncpp_dylib
	${CMAKE_COMMAND} -E copy $<TARGET_FILE:MariaDB::ConnCpp> ${PROJECT_BINARY_DIR})

add_dependencies(dDatabase conncpp_dylib)

target_include_directories(dDatabase PUBLIC ".")
target_link_libraries(dDatabase
	PUBLIC dDatabaseCDClient dDatabaseGame)
