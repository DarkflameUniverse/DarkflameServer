set(DSCRIPTS_SOURCES_AI_NP 
	"NpcNpSpacemanBob.cpp")

add_library(dScriptsAiNP STATIC ${DSCRIPTS_SOURCES_AI_NP})
target_include_directories(dScriptsAiNP PUBLIC ".")
target_precompile_headers(dScriptsAiNP REUSE_FROM dScriptsBase)
