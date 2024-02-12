set(DSERVER_SOURCES
#header
	"Server.h"

#source
	"Server.cpp")

add_library(dServer STATIC ${DSERVER_SOURCES})
target_include_directories(dServer PUBLIC .)
target_link_libraries(dServer PUBLIC dCommon)
