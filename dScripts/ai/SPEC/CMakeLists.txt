set(DSCRIPTS_SOURCES_AI_SPEC
	"SpecialCoinSpawner.cpp"
	"SpecialPowerupSpawner.cpp"
	"SpecialSpeedBuffSpawner.cpp")

add_library(dScriptsAiSpec STATIC ${DSCRIPTS_SOURCES_AI_SPEC})
target_include_directories(dScriptsAiSpec PUBLIC ".")
target_precompile_headers(dScriptsAiSpec REUSE_FROM dScriptsBase)
