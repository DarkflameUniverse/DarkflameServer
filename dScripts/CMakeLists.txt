set(DSCRIPTS_SOURCES
	"ActivityManager.cpp"
	"BaseConsoleTeleportServer.cpp"
	"BasePropertyServer.cpp"
	"BaseRandomServer.cpp"
	"BaseSurvivalServer.cpp"
	"BaseWavesGenericEnemy.cpp"
	"BaseWavesServer.cpp"
	"ChooseYourDestinationNsToNt.cpp"
	"CppScripts.cpp"
	"Darkitect.cpp"
	"NPCAddRemoveItem.cpp"
	"NtFactionSpyServer.cpp"
	"ScriptComponent.cpp"
	"ScriptedPowerupSpawner.cpp"
	"SpawnPetBaseServer.cpp")

add_subdirectory(02_server)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER})
	set(DSCRIPTS_SOURCES ${DSCRIPTS_SOURCES} "02_server/${file}")
endforeach()

add_subdirectory(ai)

foreach(file ${DSCRIPTS_SOURCES_AI})
	set(DSCRIPTS_SOURCES ${DSCRIPTS_SOURCES} "ai/${file}")
endforeach()

add_subdirectory(client)

foreach(file ${DSCRIPTS_SOURCES_CLIENT})
	set(DSCRIPTS_SOURCES ${DSCRIPTS_SOURCES} "client/${file}")
endforeach()

add_subdirectory(EquipmentScripts)

foreach(file ${DSCRIPTS_SOURCES_EQUIPMENTSCRIPTS})
	set(DSCRIPTS_SOURCES ${DSCRIPTS_SOURCES} "EquipmentScripts/${file}")
endforeach()

add_subdirectory(zone)

foreach(file ${DSCRIPTS_SOURCES_ZONE})
	set(DSCRIPTS_SOURCES ${DSCRIPTS_SOURCES} "zone/${file}")
endforeach()

set(DSCRIPTS_SOURCES ${DSCRIPTS_SOURCES} PARENT_SCOPE)
