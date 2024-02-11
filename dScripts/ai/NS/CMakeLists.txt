set(DSCRIPTS_SOURCES_AI_NS
	"ClRing.cpp"
	"NsConcertChoiceBuild.cpp"
	"NsConcertInstrument.cpp"
	"NsConcertQuickBuild.cpp"
	"NsGetFactionMissionServer.cpp"
	"NsJohnnyMissionServer.cpp"
	"NsModularBuild.cpp"
	"NsQbImaginationStatue.cpp"
	"WhFans.cpp")

add_subdirectory(NS_PP_01)

foreach(file ${DSCRIPTS_SOURCES_AI_NS_NS_PP_01})
	set(DSCRIPTS_SOURCES_AI_NS ${DSCRIPTS_SOURCES_AI_NS} "NS_PP_01/${file}")
endforeach()

add_subdirectory(WH)

foreach(file ${DSCRIPTS_SOURCES_AI_NS_WH})
	set(DSCRIPTS_SOURCES_AI_NS ${DSCRIPTS_SOURCES_AI_NS} "WH/${file}")
endforeach()

add_library(dScriptsAiNS STATIC ${DSCRIPTS_SOURCES_AI_NS})
target_include_directories(dScriptsAiNS PUBLIC "." "NS_PP_01" "WH"
PRIVATE
	${PROJECT_SOURCE_DIR}/dScripts/02_server/Map/NS) # NsConcertChoiceBuildManager.h
target_precompile_headers(dScriptsAiNS REUSE_FROM dScriptsBase)
