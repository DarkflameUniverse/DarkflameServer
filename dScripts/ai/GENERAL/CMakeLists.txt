set(DSCRIPTS_SOURCES_AI_GENERAL 
	"InstanceExitTransferPlayerToLastNonInstance.cpp"
	"LegoDieRoll.cpp")

add_library(dScriptsAiGeneral OBJECT ${DSCRIPTS_SOURCES_AI_GENERAL})
target_include_directories(dScriptsAiGeneral PUBLIC ".")
target_precompile_headers(dScriptsAiGeneral REUSE_FROM dScriptsBase)

