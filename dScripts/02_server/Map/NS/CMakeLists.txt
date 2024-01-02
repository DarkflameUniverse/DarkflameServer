set(DSCRIPTS_SOURCES_02_SERVER_MAP_NS
	"NsConcertChoiceBuildManager.cpp"
	"NsLegoClubDoor.cpp"
	"NsLupTeleport.cpp"
	"NsTokenConsoleServer.cpp")

add_subdirectory(Waves)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_MAP_NS_WAVES})
	set(DSCRIPTS_SOURCES_02_SERVER_MAP_NS ${DSCRIPTS_SOURCES_02_SERVER_MAP_NS} "Waves/${file}")
endforeach()

add_library(dScriptsServerMapNS ${DSCRIPTS_SOURCES_02_SERVER_MAP_NS})
target_include_directories(dScriptsServerMapNS PUBLIC "." "Waves")
target_precompile_headers(dScriptsServerMapNS REUSE_FROM dScriptsBase)
