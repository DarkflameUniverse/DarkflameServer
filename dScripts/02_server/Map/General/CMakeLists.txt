set(DSCRIPTS_SOURCES_02_SERVER_MAP_GENERAL
	"BankInteractServer.cpp"
	"BaseInteractDropLootServer.cpp"
	"Binoculars.cpp"
	"ExplodingAsset.cpp"
	"FrictionVolumeServer.cpp"
	"ForceVolumeServer.cpp"
	"GrowingFlower.cpp"
	"ImaginationBackpackHealServer.cpp"
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
	"VisToggleNotifierServer.cpp"
	"NTNaomiDirtServer.cpp"
	"WishingWellServer.cpp")

add_subdirectory(Ninjago)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_MAP_GENERAL_NINJAGO})
	set(DSCRIPTS_SOURCES_02_SERVER_MAP_GENERAL ${DSCRIPTS_SOURCES_02_SERVER_MAP_GENERAL} "Ninjago/${file}")
endforeach()

add_library(dScriptsServerMapGeneral ${DSCRIPTS_SOURCES_02_SERVER_MAP_GENERAL})
target_include_directories(dScriptsServerMapGeneral PUBLIC "." "Ninjago")
target_precompile_headers(dScriptsServerMapGeneral REUSE_FROM dScriptsBase)
