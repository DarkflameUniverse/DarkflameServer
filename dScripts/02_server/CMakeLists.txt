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

add_library(dScriptsServer STATIC ${DSCRIPTS_SOURCES_02_SERVER})
target_include_directories(dScriptsServer PUBLIC "."
	"DLU"
	"Equipment"
	"Minigame"
	"Minigame/General"
	"Objects"
	"Pets")
target_link_libraries(dScriptsServer INTERFACE
	dScriptsServerEnemy
	dScriptsServerMap)
target_precompile_headers(dScriptsServer REUSE_FROM dScriptsBase)
