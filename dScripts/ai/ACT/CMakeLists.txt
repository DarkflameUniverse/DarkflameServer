set(DSCRIPTS_SOURCES_AI_ACT
	"ActMine.cpp"
	"ActPlayerDeathTrigger.cpp"
	"ActVehicleDeathTrigger.cpp")

add_subdirectory(FootRace)

foreach(file ${DSCRIPTS_SOURCES_AI_ACT_FOOTRACE})
	set(DSCRIPTS_SOURCES_AI_ACT ${DSCRIPTS_SOURCES_AI_ACT} "FootRace/${file}")
endforeach()

set(DSCRIPTS_SOURCES_AI_ACT ${DSCRIPTS_SOURCES_AI_ACT} PARENT_SCOPE)
