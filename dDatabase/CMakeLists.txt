set(DDATABASE_SOURCES "CDClientDatabase.cpp"
		"CDClientManager.cpp"
		"Database.cpp"
		"MigrationRunner.cpp")

add_subdirectory(Tables)

foreach(file ${DDATABASE_TABLES_SOURCES})
	set(DDATABASE_SOURCES ${DDATABASE_SOURCES} "Tables/${file}")
endforeach()

add_library(dDatabase STATIC ${DDATABASE_SOURCES})
target_link_libraries(dDatabase sqlite3 mariadbConnCpp)
