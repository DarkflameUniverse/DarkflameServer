set(DSCRIPTS_SOURCES_AI_RACING
	"RaceImaginationServer.cpp")

add_subdirectory(OBJECTS)

foreach(file ${DSCRIPTS_SOURCES_AI_RACING_OBJECTS})
	set(DSCRIPTS_SOURCES_AI_RACING ${DSCRIPTS_SOURCES_AI_RACING} "OBJECTS/${file}")
endforeach()

add_subdirectory(TRACK_NS)

foreach(file ${DSCRIPTS_SOURCES_AI_RACING_TRACK_NS})
	set(DSCRIPTS_SOURCES_AI_RACING ${DSCRIPTS_SOURCES_AI_RACING} "TRACK_NS/${file}")
endforeach()

add_subdirectory(TRACK_NS_WINTER)

foreach(file ${DSCRIPTS_SOURCES_AI_RACING_TRACK_NS_WINTER})
	set(DSCRIPTS_SOURCES_AI_RACING ${DSCRIPTS_SOURCES_AI_RACING} "TRACK_NS_WINTER/${file}")
endforeach()

add_subdirectory(TRACK_GF)

foreach(file ${DSCRIPTS_SOURCES_AI_RACING_TRACK_GF})
	set(DSCRIPTS_SOURCES_AI_RACING ${DSCRIPTS_SOURCES_AI_RACING} "TRACK_GF/${file}")
endforeach()

add_subdirectory(TRACK_FV)

foreach(file ${DSCRIPTS_SOURCES_AI_RACING_TRACK_FV})
	set(DSCRIPTS_SOURCES_AI_RACING ${DSCRIPTS_SOURCES_AI_RACING} "TRACK_FV/${file}")
endforeach()

add_library(dScriptsAiRacing OBJECT ${DSCRIPTS_SOURCES_AI_RACING})
target_include_directories(dScriptsAiRacing PUBLIC "." "OBJECTS" "TRACK_NS" "TRACK_NS_WINTER" "TRACK_GF" "TRACK_FV")
target_precompile_headers(dScriptsAiRacing REUSE_FROM dScriptsBase)
