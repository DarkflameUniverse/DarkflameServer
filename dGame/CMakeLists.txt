set(DGAME_SOURCES "Character.cpp"
		"Entity.cpp"
		"EntityManager.cpp"
		"LeaderboardManager.cpp"
		"Player.cpp"
		"TeamManager.cpp"
		"TradingManager.cpp"
		"User.cpp"
		"UserManager.cpp")

add_subdirectory(dBehaviors)

foreach(file ${DGAME_DBEHAVIORS_SOURCES})
	set(DGAME_SOURCES ${DGAME_SOURCES} "dBehaviors/${file}")
endforeach()

add_subdirectory(dComponents)

foreach(file ${DGAME_DCOMPONENTS_SOURCES})
	set(DGAME_SOURCES ${DGAME_SOURCES} "dComponents/${file}")
endforeach()

add_subdirectory(dEntity)

foreach(file ${DGAME_DENTITY_SOURCES})
	set(DGAME_SOURCES ${DGAME_SOURCES} "dEntity/${file}")
endforeach()

add_subdirectory(dGameMessages)

foreach(file ${DGAME_DGAMEMESSAGES_SOURCES})
	set(DGAME_SOURCES ${DGAME_SOURCES} "dGameMessages/${file}")
endforeach()

add_subdirectory(dInventory)

foreach(file ${DGAME_DINVENTORY_SOURCES})
	set(DGAME_SOURCES ${DGAME_SOURCES} "dInventory/${file}")
endforeach()

add_subdirectory(dMission)

foreach(file ${DGAME_DMISSION_SOURCES})
	set(DGAME_SOURCES ${DGAME_SOURCES} "dMission/${file}")
endforeach()

add_subdirectory(dUtilities)

foreach(file ${DGAME_DUTILITIES_SOURCES})
	set(DGAME_SOURCES ${DGAME_SOURCES} "dUtilities/${file}")
endforeach()

foreach(file ${DSCRIPT_SOURCES})
	set(DGAME_SOURCES ${DGAME_SOURCES} "${PROJECT_SOURCE_DIR}/dScripts/${file}")
endforeach()

add_library(dGame STATIC ${DGAME_SOURCES})

target_link_libraries(dGame dDatabase)
