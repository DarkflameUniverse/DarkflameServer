set(DDATABASE_GAMEDATABASE_SOURCES
	"Database.cpp"
)

add_subdirectory(MySQL)

foreach(file ${DDATABSE_DATABSES_MYSQL_SOURCES})
	set(DDATABASE_GAMEDATABASE_SOURCES ${DDATABASE_GAMEDATABASE_SOURCES} "MySQL/${file}")
endforeach()

add_library(dDatabaseGame STATIC ${DDATABASE_GAMEDATABASE_SOURCES})
target_include_directories(dDatabaseGame PUBLIC "." "ITables" PRIVATE "MySQL")
target_link_libraries(dDatabaseGame
	PUBLIC mariadbConnCpp
	INTERFACE dCommon)

# Glob together all headers that need to be precompiled
file(
	GLOB HEADERS_DDATABASE_GAME
	LIST_DIRECTORIES false
	ITables/*.h
)

# Need to specify to use the CXX compiler language here or else we get errors including <string>.
target_precompile_headers(
	dDatabaseGame PRIVATE
	"$<$<COMPILE_LANGUAGE:CXX>:${HEADERS_DDATABASE_GAME}>"
)
