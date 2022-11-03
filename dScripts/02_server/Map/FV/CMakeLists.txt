set(DSCRIPTS_SOURCES_02_SERVER_MAP_FV
	"EnemyRoninSpawner.cpp"
	"FvCandle.cpp"
	"FvFong.cpp"
	"FvHorsemenTrigger.cpp"
	"ImgBrickConsoleQB.cpp")

add_subdirectory(Racing)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_MAP_FV_RACING})
	set(DSCRIPTS_SOURCES_02_SERVER_MAP_FV ${DSCRIPTS_SOURCES_02_SERVER_MAP_FV} "Racing/${file}")
endforeach()

set(DSCRIPTS_SOURCES_02_SERVER_MAP_FV ${DSCRIPTS_SOURCES_02_SERVER_MAP_FV} PARENT_SCOPE)
