set(DUGCSERVER_SOURCES
	"GZip.cpp"
)

add_library(dUgcServer ${DUGCSERVER_SOURCES})

add_executable(UgcServer "UgcServer.cpp")

target_link_libraries(dUgcServer ${COMMON_LIBRARIES})
target_link_libraries(UgcServer ${COMMON_LIBRARIES} httplib::httplib dUgcServer)
