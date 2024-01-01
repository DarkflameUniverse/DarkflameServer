set(DMASTERSERVER_SOURCES
	"InstanceManager.cpp"
	"ObjectIDManager.cpp"
)

add_library(dMasterServer ${DMASTERSERVER_SOURCES})
target_link_libraries(dMasterServer ${COMMON_LIBRARIES})

add_executable(MasterServer "MasterServer.cpp" "Start.cpp")
add_compile_definitions(MasterServer PRIVATE PROJECT_VERSION="\"${PROJECT_VERSION}\"")
target_link_libraries(MasterServer ${COMMON_LIBRARIES} dMasterServer)

option(ENABLE_K8S "Whether to enable kubernetes support")

if (${ENABLE_K8S})
	# See k8s folder
	find_package(kubernetes CONFIG REQUIRED COMPONENTS kubernetes)

	add_executable(MasterServerK8s "MasterServer.cpp" "StartK8s.cpp")
	add_compile_definitions(MasterServerK8s PRIVATE PROJECT_VERSION="\"${PROJECT_VERSION}\"")
	target_link_libraries(MasterServerK8s PRIVATE ${COMMON_LIBRARIES} dMasterServer kubernetes::kubernetes)
endif()

if(WIN32)
	add_dependencies(MasterServer WorldServer AuthServer ChatServer)
endif()
