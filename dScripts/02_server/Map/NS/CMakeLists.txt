set(DSCRIPTS_SOURCES_02_SERVER_MAP_NS
	"NsConcertChoiceBuildManager.cpp"
	"NsLegoClubDoor.cpp"
	"NsLupTeleport.cpp"
	"NsTokenConsoleServer.cpp")

add_subdirectory(Waves)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_MAP_NS_WAVES})
	set(DSCRIPTS_SOURCES_02_SERVER_MAP_NS ${DSCRIPTS_SOURCES_02_SERVER_MAP_NS} "Waves/${file}")
endforeach()

set(DSCRIPTS_SOURCES_02_SERVER_MAP_NS ${DSCRIPTS_SOURCES_02_SERVER_MAP_NS} PARENT_SCOPE)
