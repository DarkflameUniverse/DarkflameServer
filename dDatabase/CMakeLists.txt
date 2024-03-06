add_subdirectory(CDClientDatabase)
add_subdirectory(GameDatabase)

add_library(dDatabase STATIC "MigrationRunner.cpp")
target_include_directories(dDatabase PUBLIC ".")
target_link_libraries(dDatabase
	PUBLIC dDatabaseCDClient dDatabaseGame)
