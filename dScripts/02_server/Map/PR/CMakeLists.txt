set(DSCRIPTS_SOURCES_02_SERVER_MAP_PR
	"HydrantBroken.cpp"
	"PrSeagullFly.cpp"
	"SpawnGryphonServer.cpp")

add_library(dScriptsServerMapPR ${DSCRIPTS_SOURCES_02_SERVER_MAP_PR})
target_include_directories(dScriptsServerMapPR PUBLIC ".")
target_precompile_headers(dScriptsServerMapPR REUSE_FROM dScriptsBase)
