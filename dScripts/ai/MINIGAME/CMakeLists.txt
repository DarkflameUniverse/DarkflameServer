set(DSCRIPTS_SOURCES_AI_MINIGAME)

add_subdirectory(SG_GF)

foreach(file ${DSCRIPTS_SOURCES_AI_MINIGAME_SG_GF})
	set(DSCRIPTS_SOURCES_AI_MINIGAME ${DSCRIPTS_SOURCES_AI_MINIGAME} "SG_GF/${file}")
endforeach()

add_subdirectory(Objects)

foreach(file ${DSCRIPTS_SOURCES_AI_MINIGAME_OBJECTS})
	set(DSCRIPTS_SOURCES_AI_MINIGAME ${DSCRIPTS_SOURCES_AI_MINIGAME} "Objects/${file}")
endforeach()

add_library(dScriptsAiMinigame OBJECT ${DSCRIPTS_SOURCES_AI_MINIGAME})
target_include_directories(dScriptsAiMinigame PUBLIC "." "Objects" "SG_GF" "SG_GF/SERVER")
target_precompile_headers(dScriptsAiMinigame REUSE_FROM dScriptsBase)
