set(DSERVER_SOURCES
	"Server.cpp")

add_library(dServer STATIC ${DSERVER_SOURCES})

target_include_directories(dServer PUBLIC ".")
