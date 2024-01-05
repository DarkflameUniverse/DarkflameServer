set(DSCRIPTS_SOURCES_02_SERVER_MAP_GF 
	"GfTikiTorch.cpp"
	"GfCaptainsCannon.cpp"
	"MastTeleport.cpp"
	"SpawnLionServer.cpp")

add_library(dScriptsServerMapGF ${DSCRIPTS_SOURCES_02_SERVER_MAP_GF})
target_include_directories(dScriptsServerMapGF PUBLIC ".")
target_precompile_headers(dScriptsServerMapGF REUSE_FROM dScriptsBase)
