set(DSCRIPTS_SOURCES_CLIENT)

add_subdirectory(ai)

foreach(file ${DSCRIPTS_SOURCES_CLIENT_AI})
	set(DSCRIPTS_SOURCES_CLIENT ${DSCRIPTS_SOURCES_CLIENT} "ai/${file}")
endforeach()

add_library(dScriptsClient OBJECT ${DSCRIPTS_SOURCES_CLIENT})
target_include_directories(dScriptsClient PUBLIC "." "ai" "ai/PR")
target_precompile_headers(dScriptsClient REUSE_FROM dScriptsBase)
