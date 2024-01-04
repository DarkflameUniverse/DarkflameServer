set(DSCRIPTS_SOURCES_02_SERVER)

add_subdirectory(DLU)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_DLU})
	set(DSCRIPTS_SOURCES_02_SERVER ${DSCRIPTS_SOURCES_02_SERVER} "DLU/${file}")
endforeach()

add_subdirectory(Enemy)

add_subdirectory(Equipment)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_EQUIPMENT})
	set(DSCRIPTS_SOURCES_02_SERVER ${DSCRIPTS_SOURCES_02_SERVER} "Equipment/${file}")
endforeach()

add_subdirectory(Map)

add_subdirectory(Minigame)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_MINIGAME})
	set(DSCRIPTS_SOURCES_02_SERVER ${DSCRIPTS_SOURCES_02_SERVER} "Minigame/${file}")
endforeach()

add_subdirectory(Objects)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_OBJECTS})
	set(DSCRIPTS_SOURCES_02_SERVER ${DSCRIPTS_SOURCES_02_SERVER} "Objects/${file}")
endforeach()

add_subdirectory(Pets)

add_library(dScriptsServerBase OBJECT ${DSCRIPTS_SOURCES_02_SERVER})
target_include_directories(dScriptsServerBase PUBLIC "."
	"DLU"
	"Equipment"
	"Minigame"
	"Minigame/General"
	"Objects"
)
target_precompile_headers(dScriptsServerBase REUSE_FROM dScriptsBase)

add_library(dScriptsServer INTERFACE)
target_sources(dScriptsServer INTERFACE
	$<TARGET_OBJECTS:dScriptsServerBase>
	$<TARGET_OBJECTS:dScriptsServerEnemy>
	$<TARGET_OBJECTS:dScriptsServerPets>
)
target_link_libraries(dScriptsServer INTERFACE
	dScriptsServerMap
)
target_include_directories(dScriptsServer INTERFACE
	$<TARGET_PROPERTY:dScriptsServerBase,INTERFACE_INCLUDE_DIRECTORIES>
	$<TARGET_PROPERTY:dScriptsServerEnemy,INTERFACE_INCLUDE_DIRECTORIES>
	$<TARGET_PROPERTY:dScriptsServerMap,INTERFACE_INCLUDE_DIRECTORIES>
	$<TARGET_PROPERTY:dScriptsServerPets,INTERFACE_INCLUDE_DIRECTORIES>
)

