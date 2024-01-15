set(DSCRIPTS_SOURCES_02_SERVER_MAP_VE 
	"VeMissionConsole.cpp"
	"VeEpsilonServer.cpp"
	"VeBricksampleServer.cpp")

add_library(dScriptsServerMapVE ${DSCRIPTS_SOURCES_02_SERVER_MAP_VE})
target_include_directories(dScriptsServerMapVE PUBLIC ".")
target_precompile_headers(dScriptsServerMapVE REUSE_FROM dScriptsBase)
