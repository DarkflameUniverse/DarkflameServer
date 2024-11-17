set(DSCRIPTS_SOURCES_AI_AG 
	"AgShipPlayerDeathTrigger.cpp"
	"AgSpaceStuff.cpp"
    "AgShipShake.cpp"
	"AgShipPlayerShockServer.cpp"
	"AgImagSmashable.cpp"
	"ActSharkPlayerDeathTrigger.cpp"
	"AgBusDoor.cpp"
	"AgTurret.cpp"
	"AgFans.cpp"
	"AgSalutingNpcs.cpp"
	"AgJetEffectServer.cpp"
	"AgQbElevator.cpp"
	"AgStromlingProperty.cpp"
	"AgDarkSpiderling.cpp"
	"AgPicnicBlanket.cpp"
	"AgStagePlatforms.cpp"
	"AgQbWall.cpp")

add_library(dScriptsAiAG OBJECT ${DSCRIPTS_SOURCES_AI_AG})
target_include_directories(dScriptsAiAG PUBLIC ".")
target_precompile_headers(dScriptsAiAG REUSE_FROM dScriptsBase)
