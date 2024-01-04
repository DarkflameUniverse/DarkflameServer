set(DSCRIPTS_SOURCES_AI_RACING)

add_subdirectory(OBJECTS)

foreach(file ${DSCRIPTS_SOURCES_AI_RACING_OBJECTS})
	set(DSCRIPTS_SOURCES_AI_RACING ${DSCRIPTS_SOURCES_AI_RACING} "OBJECTS/${file}")
endforeach()

add_library(dScriptsAiRacing STATIC ${DSCRIPTS_SOURCES_AI_RACING})
target_include_directories(dScriptsAiRacing PUBLIC "." "OBJECTS")
target_precompile_headers(dScriptsAiRacing REUSE_FROM dScriptsBase)
