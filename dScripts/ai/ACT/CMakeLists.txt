set(DSCRIPTS_SOURCES_AI_ACT
	"ActMine.cpp"
	"ActPlayerDeathTrigger.cpp"
	"ActVehicleDeathTrigger.cpp")

add_subdirectory(FootRace)

foreach(file ${DSCRIPTS_SOURCES_AI_ACT_FOOTRACE})
	set(DSCRIPTS_SOURCES_AI_ACT ${DSCRIPTS_SOURCES_AI_ACT} "FootRace/${file}")
endforeach()

add_library(dScriptsAiAct OBJECT ${DSCRIPTS_SOURCES_AI_ACT})
target_include_directories(dScriptsAiAct PUBLIC "." "FootRace")
target_precompile_headers(dScriptsAiAct REUSE_FROM dScriptsBase)
