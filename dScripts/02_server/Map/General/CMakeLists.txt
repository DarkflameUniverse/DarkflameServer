set(DSCRIPTS_SOURCES_02_SERVER_MAP_GENERAL
	"BankInteractServer.cpp"
	"BaseInteractDropLootServer.cpp"
	"Binoculars.cpp"
	"ExplodingAsset.cpp"
	"ForceVolumeServer.cpp"
	"GrowingFlower.cpp"
	"ImaginationBackpackHealServer.cpp"
	"InvalidScript.cpp"
	"MailBoxServer.cpp"
	"NjRailSwitch.cpp"
	"PetDigServer.cpp"
	"PropertyDevice.cpp"
	"PropertyPlatform.cpp"
	"QbEnemyStunner.cpp"
	"QbSpawner.cpp"
	"StoryBoxInteractServer.cpp"
	"TokenConsoleServer.cpp"
	"TouchMissionUpdateServer.cpp"
	"WishingWellServer.cpp")

add_subdirectory(Ninjago)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_MAP_GENERAL_NINJAGO})
	set(DSCRIPTS_SOURCES_02_SERVER_MAP_GENERAL ${DSCRIPTS_SOURCES_02_SERVER_MAP_GENERAL} "Ninjago/${file}")
endforeach()

set(DSCRIPTS_SOURCES_02_SERVER_MAP_GENERAL ${DSCRIPTS_SOURCES_02_SERVER_MAP_GENERAL} PARENT_SCOPE)
