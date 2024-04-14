set(DSCRIPTS_SOURCES_AI_GF 
	"GfCampfire.cpp"
	"GfOrgan.cpp"
	"GfBanana.cpp"
	"GfBananaCluster.cpp"
	"GfJailkeepMission.cpp"
	"TriggerAmbush.cpp"
	"GfJailWalls.cpp"
	"PetDigBuild.cpp"
	"GfArchway.cpp"
	"GfMaelstromGeyser.cpp"
	"PirateRep.cpp"
	"GfParrotCrash.cpp")

add_library(dScriptsAiGF OBJECT ${DSCRIPTS_SOURCES_AI_GF})
target_include_directories(dScriptsAiGF PUBLIC ".")
target_precompile_headers(dScriptsAiGF REUSE_FROM dScriptsBase)
