set(DSCRIPTS_SOURCES_AI_PETS 
	"HydrantSmashable.cpp")

add_library(dScriptsAiPets STATIC ${DSCRIPTS_SOURCES_AI_PETS})
target_include_directories(dScriptsAiPets PUBLIC "." "NS_PP_01" "WH")
target_precompile_headers(dScriptsAiPets REUSE_FROM dScriptsBase)
