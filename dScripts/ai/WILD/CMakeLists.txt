set(DSCRIPTS_SOURCES_AI_WILD
	"AllCrateChicken.cpp"
	"LupGenericInteract.cpp"
	"WildAmbients.cpp"
	"WildAmbientCrab.cpp"
	"WildAndScared.cpp"
	"WildGfGlowbug.cpp"
	"WildNinjaBricks.cpp"
	"WildNinjaStudent.cpp"
	"WildNinjaSensei.cpp"
	"WildPants.cpp")

add_library(dScriptsAiWild OBJECT ${DSCRIPTS_SOURCES_AI_WILD})
target_include_directories(dScriptsAiWild PUBLIC ".")
target_precompile_headers(dScriptsAiWild REUSE_FROM dScriptsBase)
