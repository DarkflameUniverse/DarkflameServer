set(DSCRIPTS_SOURCES_02_SERVER)

add_subdirectory(DLU)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_DLU})
	set(DSCRIPTS_SOURCES_02_SERVER ${DSCRIPTS_SOURCES_02_SERVER} "DLU/${file}")
endforeach()

add_subdirectory(Enemy)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_ENEMY})
	set(DSCRIPTS_SOURCES_02_SERVER ${DSCRIPTS_SOURCES_02_SERVER} "Enemy/${file}")
endforeach()

add_subdirectory(Equipment)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_EQUIPMENT})
	set(DSCRIPTS_SOURCES_02_SERVER ${DSCRIPTS_SOURCES_02_SERVER} "Equipment/${file}")
endforeach()

add_subdirectory(Map)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_MAP})
	set(DSCRIPTS_SOURCES_02_SERVER ${DSCRIPTS_SOURCES_02_SERVER} "Map/${file}")
endforeach()

add_subdirectory(Minigame)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_MINIGAME})
	set(DSCRIPTS_SOURCES_02_SERVER ${DSCRIPTS_SOURCES_02_SERVER} "Minigame/${file}")
endforeach()

add_subdirectory(Objects)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_OBJECTS})
	set(DSCRIPTS_SOURCES_02_SERVER ${DSCRIPTS_SOURCES_02_SERVER} "Objects/${file}")
endforeach()

add_subdirectory(Pets)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_PETS})
	set(DSCRIPTS_SOURCES_02_SERVER ${DSCRIPTS_SOURCES_02_SERVER} "Pets/${file}")
endforeach()

set(DSCRIPTS_SOURCES_02_SERVER ${DSCRIPTS_SOURCES_02_SERVER} PARENT_SCOPE)
