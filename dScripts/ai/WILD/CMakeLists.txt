set(DSCRIPTS_SOURCES_AI_WILD
	"AllCrateChicken.cpp"
	"WildAmbients.cpp"
	"WildAmbientCrab.cpp"
	"WildAndScared.cpp"
	"WildGfGlowbug.cpp"
	"WildNinjaBricks.cpp"
	"WildNinjaStudent.cpp"
	"WildNinjaSensei.cpp"
	"WildPants.cpp")

add_library(dScriptsAiWild STATIC ${DSCRIPTS_SOURCES_AI_WILD})
target_include_directories(dScriptsAiWild PUBLIC ".")
target_precompile_headers(dScriptsAiWild REUSE_FROM dScriptsBase)
