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

add_library(dScriptsAiFV OBJECT ${DSCRIPTS_SOURCES_AI_FV})
target_include_directories(dScriptsAiFV PUBLIC ".")
target_precompile_headers(dScriptsAiFV REUSE_FROM dScriptsBase)

