set(DSCRIPTS_SOURCES_02_SERVER_MAP_AM 
	"AmConsoleTeleportServer.cpp"
	"RandomSpawnerFin.cpp"
	"RandomSpawnerPit.cpp"
	"RandomSpawnerStr.cpp"
	"RandomSpawnerZip.cpp"
	"AmBridge.cpp"
	"AmDrawBridge.cpp"
	"AmShieldGenerator.cpp"
	"AmShieldGeneratorQuickbuild.cpp"
	"AmDropshipComputer.cpp"
	"AmScrollReaderServer.cpp"
	"AmTemplateSkillVolume.cpp"
	"AmSkullkinDrill.cpp"
	"AmSkullkinDrillStand.cpp"
	"AmSkullkinTower.cpp"
	"AmBlueX.cpp"
	"AmTeapotServer.cpp"
	"WanderingVendor.cpp"
	)

add_library(dScriptsServerMapAM OBJECT ${DSCRIPTS_SOURCES_02_SERVER_MAP_AM})
target_include_directories(dScriptsServerMapAM PUBLIC ".")
target_precompile_headers(dScriptsServerMapAM REUSE_FROM dScriptsBase)
