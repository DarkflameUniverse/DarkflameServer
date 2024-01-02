set(DSCRIPTS_SOURCES_02_SERVER_ENEMY)

add_subdirectory(AG)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_ENEMY_AG})
	set(DSCRIPTS_SOURCES_02_SERVER_ENEMY ${DSCRIPTS_SOURCES_02_SERVER_ENEMY} "AG/${file}")
endforeach()

add_subdirectory(AM)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_ENEMY_AM})
	set(DSCRIPTS_SOURCES_02_SERVER_ENEMY ${DSCRIPTS_SOURCES_02_SERVER_ENEMY} "AM/${file}")
endforeach()

add_subdirectory(FV)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_ENEMY_FV})
	set(DSCRIPTS_SOURCES_02_SERVER_ENEMY ${DSCRIPTS_SOURCES_02_SERVER_ENEMY} "FV/${file}")
endforeach()

add_subdirectory(General)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_ENEMY_GENERAL})
	set(DSCRIPTS_SOURCES_02_SERVER_ENEMY ${DSCRIPTS_SOURCES_02_SERVER_ENEMY} "General/${file}")
endforeach()

add_subdirectory(Survival)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_ENEMY_SURVIVAL})
	set(DSCRIPTS_SOURCES_02_SERVER_ENEMY ${DSCRIPTS_SOURCES_02_SERVER_ENEMY} "Survival/${file}")
endforeach()

add_subdirectory(VE)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_ENEMY_VE})
	set(DSCRIPTS_SOURCES_02_SERVER_ENEMY ${DSCRIPTS_SOURCES_02_SERVER_ENEMY} "VE/${file}")
endforeach()

add_subdirectory(Waves)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_ENEMY_WAVES})
	set(DSCRIPTS_SOURCES_02_SERVER_ENEMY ${DSCRIPTS_SOURCES_02_SERVER_ENEMY} "Waves/${file}")
endforeach()

add_library(dScriptsServerEnemy STATIC ${DSCRIPTS_SOURCES_02_SERVER_ENEMY})
target_link_libraries(dScriptsServerEnemy dScriptsBase)
target_include_directories(dScriptsServerEnemy PUBLIC "."
	"AG"
	"AM"
	"FV"
	"General"
	"Survival"
	"VE"
	"Waves"
)
target_precompile_headers(dScriptsServerEnemy PRIVATE ${HEADERS_DGAME})
