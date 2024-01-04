set(DSCRIPTS_SOURCES_ZONE)

add_subdirectory(AG)

foreach(file ${DSCRIPTS_SOURCES_ZONE_AG})
	set(DSCRIPTS_SOURCES_ZONE ${DSCRIPTS_SOURCES_ZONE} "AG/${file}")
endforeach()

add_subdirectory(LUPs)

foreach(file ${DSCRIPTS_SOURCES_ZONE_LUPS})
	set(DSCRIPTS_SOURCES_ZONE ${DSCRIPTS_SOURCES_ZONE} "LUPs/${file}")
endforeach()

add_subdirectory(PROPERTY)

foreach(file ${DSCRIPTS_SOURCES_ZONE_PROPERTY})
	set(DSCRIPTS_SOURCES_ZONE ${DSCRIPTS_SOURCES_ZONE} "PROPERTY/${file}")
endforeach()

add_library(dScriptsZone OBJECT ${DSCRIPTS_SOURCES_ZONE})
target_include_directories(dScriptsZone PUBLIC "."
	"AG"
	"LUPs"
	"PROPERTY"
	"PROPERTY/FV"
	"PROPERTY/GF"
	"PROPERTY/NS")
target_precompile_headers(dScriptsZone REUSE_FROM dScriptsBase)
