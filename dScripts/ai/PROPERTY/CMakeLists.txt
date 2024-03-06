set(DSCRIPTS_SOURCES_AI_PROPERTY
	"AgPropguards.cpp"
	"PropertyFXDamage.cpp")

add_subdirectory(AG)

foreach(file ${DSCRIPTS_SOURCES_AI_PROPERTY_AG})
	set(DSCRIPTS_SOURCES_AI_PROPERTY ${DSCRIPTS_SOURCES_AI_PROPERTY} "AG/${file}")
endforeach()

add_library(dScriptsAiProperty OBJECT ${DSCRIPTS_SOURCES_AI_PROPERTY})
target_include_directories(dScriptsAiProperty PUBLIC "." "AG")
target_precompile_headers(dScriptsAiProperty REUSE_FROM dScriptsBase)
