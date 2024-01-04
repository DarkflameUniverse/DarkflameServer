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

add_library(dScriptsServerMapFV OBJECT ${DSCRIPTS_SOURCES_02_SERVER_MAP_FV})
target_include_directories(dScriptsServerMapFV PUBLIC "." "Racing")
target_precompile_headers(dScriptsServerMapFV REUSE_FROM dScriptsBase)
