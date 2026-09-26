set(DSCRIPTS_SOURCES_AI_FV
	"ActNinjaSensei.cpp"
	"ActNinjaTurret.cpp"
	"FvFlyingCreviceDragon.cpp"
	"FvDragonSmashingGolemQb.cpp"
	"FvFreeGfNinjas.cpp"
	"FvPandaSpawnerServer.cpp"
	"FvPandaServer.cpp"
	"FvBrickPuzzleServer.cpp"
	"FvConsoleLeftQuickbuild.cpp"
	"FvConsoleRightQuickbuild.cpp"
	"FvFacilityBrick.cpp"
	"FvFacilityPipes.cpp"
	"ActParadoxPipeFix.cpp"
	"FvNinjaGuard.cpp"
	"FvPassThroughWall.cpp"
	"FvBounceOverWall.cpp"
	"FvMaelstromGeyser.cpp"
	"TriggerGas.cpp")

add_subdirectory(Dragon_Instance)

foreach(file ${DSCRIPTS_SOURCES_AI_FV_DRAGON_INSTANCE})
	set(DSCRIPTS_SOURCES_AI_FV ${DSCRIPTS_SOURCES_AI_FV} "Dragon_Instance/${file}")
endforeach()

add_library(dScriptsAiFV OBJECT ${DSCRIPTS_SOURCES_AI_FV})
target_include_directories(dScriptsAiFV PUBLIC "." "Dragon_Instance")
target_precompile_headers(dScriptsAiFV REUSE_FROM dScriptsBase)

