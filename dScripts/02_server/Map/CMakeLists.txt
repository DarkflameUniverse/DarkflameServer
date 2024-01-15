add_subdirectory(AG)
add_subdirectory(AG_Spider_Queen)
add_subdirectory(AM)
add_subdirectory(FV)
add_subdirectory(General)
add_subdirectory(GF)
add_subdirectory(njhub)
add_subdirectory(NS)
add_subdirectory(NT)
add_subdirectory(PR)
add_subdirectory(Property)
add_subdirectory(SS)
add_subdirectory(VE)

add_library(dScriptsServerMap INTERFACE)
target_link_libraries(dScriptsServerMap INTERFACE
	dScriptsServerMapAG
	dScriptsServerMapAGSpiderQueen
	dScriptsServerMapAM
	dScriptsServerMapFV
	dScriptsServerMapGeneral
	dScriptsServerMapGF
	dScriptsServerMapNJHub
	dScriptsServerMapNS
	dScriptsServerMapNT
	dScriptsServerMapPR
	dScriptsServerMapProperty
	dScriptsServerMapSS
	dScriptsServerMapVE)
