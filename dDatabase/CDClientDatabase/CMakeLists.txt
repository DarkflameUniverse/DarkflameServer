set(DDATABASE_CDCLIENTDATABASE_SOURCES
	"CDClientDatabase.cpp"
	"CDClientManager.cpp"
)

add_subdirectory(CDClientTables)

foreach(file ${DDATABASE_CDCLIENTDATABASE_CDCLIENTTABLES_SOURCES})
	set(DDATABASE_CDCLIENTDATABASE_SOURCES ${DDATABASE_CDCLIENTDATABASE_SOURCES} "CDClientTables/${file}")
endforeach()

add_library(dDatabaseCDClient STATIC ${DDATABASE_CDCLIENTDATABASE_SOURCES})
target_include_directories(dDatabaseCDClient PUBLIC "." "CDClientTables")
target_link_libraries(dDatabaseCDClient PRIVATE sqlite3)

if (${CDCLIENT_CACHE_ALL})
	add_compile_definitions(dDatabaseCDClient PRIVATE CDCLIENT_CACHE_ALL=${CDCLIENT_CACHE_ALL})
endif()

file(
	GLOB HEADERS_DDATABASE_CDCLIENT
	LIST_DIRECTORIES false
	${PROJECT_SOURCE_DIR}/thirdparty/SQLite/*.h
	CDClientTables/*.h
	*.h
)

# Need to specify to use the CXX compiler language here or else we get errors including <string>.
target_precompile_headers(
	dDatabaseCDClient PRIVATE
	"$<$<COMPILE_LANGUAGE:CXX>:${HEADERS_DDATABASE_CDCLIENT}>"
)
