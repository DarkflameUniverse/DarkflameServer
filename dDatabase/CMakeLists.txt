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
target_link_libraries(dDatabase sqlite3 mariadbConnCpp)

# If using gcc, warn on old-style C casts
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	set_source_files_properties(${DDATABASE_SOURCES} PROPERTIES COMPILE_FLAGS "-Wold-style-cast")
endif()
