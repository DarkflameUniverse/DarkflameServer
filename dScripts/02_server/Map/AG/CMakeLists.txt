set(DSCRIPTS_SOURCES_02_SERVER_MAP_AG 
	"AgCagedBricksServer.cpp"
	"NpcWispServer.cpp"
	"NpcEpsilonServer.cpp"
	"AgLaserSensorServer.cpp"
	"AgMonumentLaserServer.cpp"
	"AgMonumentBirds.cpp"
	"RemoveRentalGear.cpp"
	"NpcNjAssistantServer.cpp"
	"AgBugsprayer.cpp"
	"NpcAgCourseStarter.cpp"
	"AgMonumentRaceGoal.cpp"
	"AgMonumentRaceCancel.cpp"
	"NpcCowboyServer.cpp"
	"NpcPirateServer.cpp")

add_library(dScriptsServerMapAG ${DSCRIPTS_SOURCES_02_SERVER_MAP_AG})
target_include_directories(dScriptsServerMapAG PUBLIC ".")
target_precompile_headers(dScriptsServerMapAG REUSE_FROM dScriptsBase)
