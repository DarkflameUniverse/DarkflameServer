set(DSCRIPTS_SOURCES_02_SERVER_MAP_NJHUB
	"BurningTile.cpp"
	"CatapultBaseServer.cpp"
	"CatapultBouncerServer.cpp"
	"CavePrisonCage.cpp"
	"EnemySkeletonSpawner.cpp"
	"FallingTile.cpp"
	"FlameJetServer.cpp"
	"ImaginationShrineServer.cpp"
	"Lieutenant.cpp"
	"MonCoreNookDoors.cpp"
	"MonCoreSmashableDoors.cpp"
	"NjColeNPC.cpp"
	"NjDragonEmblemChestServer.cpp"
	"NjEarthDragonPetServer.cpp"
	"NjEarthPetServer.cpp"
	"NjGarmadonCelebration.cpp"
	"NjJayMissionItems.cpp"
	"NjNPCMissionSpinjitzuServer.cpp"
	"NjNyaMissionitems.cpp"
	"NjScrollChestServer.cpp"
	"NjWuNPC.cpp"
	"RainOfArrows.cpp")

add_subdirectory(boss_instance)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_MAP_NJHUB_BOSS_INSTANCE})
	set(DSCRIPTS_SOURCES_02_SERVER_MAP_NJHUB ${DSCRIPTS_SOURCES_02_SERVER_MAP_NJHUB} "boss_instance/${file}")
endforeach()

set(DSCRIPTS_SOURCES_02_SERVER_MAP_NJHUB ${DSCRIPTS_SOURCES_02_SERVER_MAP_NJHUB} PARENT_SCOPE)
