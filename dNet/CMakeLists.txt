set(DNET_SOURCES "AuthPackets.cpp"
	"ChatPackets.cpp"
	"ClientPackets.cpp"
	"dServer.cpp"
	"MasterPackets.cpp"
	"PacketUtils.cpp"
	"WorldPackets.cpp"
	"RakNetTransportLayer.cpp"
	"RakNetPeer.cpp"
	"ZoneInstanceManager.cpp")

find_package(OpenSSL REQUIRED)

# Find boost for asio
cmake_policy(SET CMP0167 NEW)
find_package(Boost QUIET COMPONENTS system)

if(Boost_FOUND)
	message(STATUS "Boost found, enabling TCP transport support")

	# Add to source list
	list(APPEND DNET_SOURCES "TcpTransportLayer.cpp" "TcpPeer.cpp" "TcpSession.cpp")
endif()

add_library(dNet STATIC ${DNET_SOURCES})

if(Boost_FOUND)
	target_compile_definitions(dNet PRIVATE NET_ENABLE_TCP_TRANSPORT)

	target_link_libraries(dNet PRIVATE bcrypt MD5 OpenSSL::SSL OpenSSL::Crypto OpenSSL::applink Boost::system)
else()
	target_link_libraries(dNet PRIVATE bcrypt MD5)
endif()

target_include_directories(dNet PRIVATE
	"${PROJECT_SOURCE_DIR}/dCommon"
	"${PROJECT_SOURCE_DIR}/dCommon/dEnums"

	"${PROJECT_SOURCE_DIR}/dZoneManager"

	"${PROJECT_SOURCE_DIR}/dDatabase/CDClientDatabase"
	"${PROJECT_SOURCE_DIR}/dDatabase/CDClientDatabase/CDClientTables"
	"${PROJECT_SOURCE_DIR}/dDatabase/GameDatabase"
	"${PROJECT_SOURCE_DIR}/dDatabase/GameDatabase/ITables"
	"${PROJECT_SOURCE_DIR}/thirdparty/mariadb-connector-cpp/include"

	"${PROJECT_SOURCE_DIR}/dGame" # UserManager.h
	"${PROJECT_SOURCE_DIR}/dGame/dComponents"
	"${PROJECT_SOURCE_DIR}/dGame/dEntity" # via dZoneManager
	"${PROJECT_SOURCE_DIR}/dGame/dGameMessages" # GameMessages.h
	"${PROJECT_SOURCE_DIR}/dGame/dInventory" # via PossessableComponent.h
	"${PROJECT_SOURCE_DIR}/dGame/dUtilities" # via Item.h
	"${PROJECT_SOURCE_DIR}/dScripts" # transitive through components
)
