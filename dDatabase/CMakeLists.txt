set(DDATABASE_SOURCES)

add_subdirectory(CDClientDatabase)

foreach(file ${DDATABASE_CDCLIENTDATABASE_SOURCES})
	set(DDATABASE_SOURCES ${DDATABASE_SOURCES} "CDClientDatabase/${file}")
endforeach()

add_subdirectory(GameDatabase)

foreach(file ${DDATABASE_GAMEDATABASE_SOURCES})
	set(DDATABASE_SOURCES ${DDATABASE_SOURCES} "GameDatabase/${file}")
endforeach()

add_library(dDatabase STATIC ${DDATABASE_SOURCES})
target_include_directories(dDatabase PUBLIC
.
./CDClientDatabase
./CDClientDatabase/CDClientTables

./GameDatabase
./GameDatabase/ITables
./GameDatabase/MySQL
./GameDatabase/MySQL/Tables
)

target_link_libraries(dDatabase PUBLIC sqlite3 mariadbConnCpp dCommon)

if (${CDCLIENT_CACHE_ALL})
	add_compile_definitions(dDatabase CDCLIENT_CACHE_ALL=${CDCLIENT_CACHE_ALL})
endif()
