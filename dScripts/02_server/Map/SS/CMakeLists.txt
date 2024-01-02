set(DSCRIPTS_SOURCES_02_SERVER_MAP_SS 
	"SsModularBuildServer.cpp")

add_library(dScriptsServerMapSS ${DSCRIPTS_SOURCES_02_SERVER_MAP_SS})
target_include_directories(dScriptsServerMapSS PUBLIC ".")
target_precompile_headers(dScriptsServerMapSS REUSE_FROM dScriptsBase)
