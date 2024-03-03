set(DSERVER_SOURCES
	"Server.cpp")

add_library(dServer STATIC ${DSERVER_SOURCES})

target_include_directories(dServer PUBLIC ".")

target_include_directories(dServer PRIVATE
	${PROJECT_SOURCE_DIR}/dCommon/ # BinaryPathFinder.h
)
