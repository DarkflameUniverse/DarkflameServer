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
target_sources(dScriptsServerMap INTERFACE
	$<TARGET_OBJECTS:dScriptsServerMapAG>
	$<TARGET_OBJECTS:dScriptsServerMapAGSpiderQueen>
	$<TARGET_OBJECTS:dScriptsServerMapAM>
	$<TARGET_OBJECTS:dScriptsServerMapFV>
	$<TARGET_OBJECTS:dScriptsServerMapGeneral>
	$<TARGET_OBJECTS:dScriptsServerMapGF>
	$<TARGET_OBJECTS:dScriptsServerMapNJHub>
	$<TARGET_OBJECTS:dScriptsServerMapNS>
	$<TARGET_OBJECTS:dScriptsServerMapNT>
	$<TARGET_OBJECTS:dScriptsServerMapPR>
	$<TARGET_OBJECTS:dScriptsServerMapProperty>
	$<TARGET_OBJECTS:dScriptsServerMapSS>
	$<TARGET_OBJECTS:dScriptsServerMapVE>
)
target_include_directories(dScriptsServerMap INTERFACE
	$<TARGET_PROPERTY:dScriptsServerMapAG,INTERFACE_INCLUDE_DIRECTORIES>
	$<TARGET_PROPERTY:dScriptsServerMapAGSpiderQueen,INTERFACE_INCLUDE_DIRECTORIES>
	$<TARGET_PROPERTY:dScriptsServerMapAM,INTERFACE_INCLUDE_DIRECTORIES>
	$<TARGET_PROPERTY:dScriptsServerMapFV,INTERFACE_INCLUDE_DIRECTORIES>
	$<TARGET_PROPERTY:dScriptsServerMapGeneral,INTERFACE_INCLUDE_DIRECTORIES>
	$<TARGET_PROPERTY:dScriptsServerMapGF,INTERFACE_INCLUDE_DIRECTORIES>
	$<TARGET_PROPERTY:dScriptsServerMapNJHub,INTERFACE_INCLUDE_DIRECTORIES>
	$<TARGET_PROPERTY:dScriptsServerMapNS,INTERFACE_INCLUDE_DIRECTORIES>
	$<TARGET_PROPERTY:dScriptsServerMapNT,INTERFACE_INCLUDE_DIRECTORIES>
	$<TARGET_PROPERTY:dScriptsServerMapPR,INTERFACE_INCLUDE_DIRECTORIES>
	$<TARGET_PROPERTY:dScriptsServerMapProperty,INTERFACE_INCLUDE_DIRECTORIES>
	$<TARGET_PROPERTY:dScriptsServerMapSS,INTERFACE_INCLUDE_DIRECTORIES>
	$<TARGET_PROPERTY:dScriptsServerMapVE,INTERFACE_INCLUDE_DIRECTORIES>
)
