set(DSCRIPTS_SOURCES_02_SERVER_MAP_NT
	"NtCombatChallengeDummy.cpp"
	"NtCombatChallengeExplodingDummy.cpp"
	"NtCombatChallengeServer.cpp"
	"NtAssemblyTubeServer.cpp"
	"NtParadoxPanelServer.cpp"
	"NtImagBeamBuffer.cpp"
	"NtBeamImaginationCollectors.cpp"
	"NtDirtCloudServer.cpp"
	"NtConsoleTeleportServer.cpp"
	"SpawnStegoServer.cpp"
	"SpawnSaberCatServer.cpp"
	"SpawnShrakeServer.cpp"
	"NtDukeServer.cpp"
	"NtHaelServer.cpp"
	"NtOverbuildServer.cpp"
	"NtVandaServer.cpp"
	"NtXRayServer.cpp"
	"NtSleepingGuard.cpp"
	"NtImagimeterVisibility.cpp"
	"NTPipeVisibilityServer.cpp"
	"NtSentinelWalkwayServer.cpp"
	"NtDarkitectRevealServer.cpp"
	"NtParadoxTeleServer.cpp"
	"NtVentureSpeedPadServer.cpp"
	"NtVentureCannonServer.cpp"
	"NtBcSubmitServer.cpp"
	"NtNaomiBreadcrumbServer.cpp")

add_library(dScriptsServerMapNT OBJECT ${DSCRIPTS_SOURCES_02_SERVER_MAP_NT})
target_include_directories(dScriptsServerMapNT PUBLIC ".")
target_precompile_headers(dScriptsServerMapNT REUSE_FROM dScriptsBase)
