set(DSCRIPTS_SOURCES_02_SERVER_MAP_PROPERTY
	"PropertyBankInteract.cpp")

add_subdirectory(AG_Med)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_MAP_PROPERTY_AG_MED})
	set(DSCRIPTS_SOURCES_02_SERVER_MAP_PROPERTY ${DSCRIPTS_SOURCES_02_SERVER_MAP_PROPERTY} "AG_Med/${file}")
endforeach()

add_subdirectory(AG_Small)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_MAP_PROPERTY_AG_SMALL})
	set(DSCRIPTS_SOURCES_02_SERVER_MAP_PROPERTY ${DSCRIPTS_SOURCES_02_SERVER_MAP_PROPERTY} "AG_Small/${file}")
endforeach()

add_subdirectory(NS_Med)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_MAP_PROPERTY_NS_MED})
	set(DSCRIPTS_SOURCES_02_SERVER_MAP_PROPERTY ${DSCRIPTS_SOURCES_02_SERVER_MAP_PROPERTY} "NS_Med/${file}")
endforeach()

add_library(dScriptsServerMapProperty OBJECT ${DSCRIPTS_SOURCES_02_SERVER_MAP_PROPERTY})
target_precompile_headers(dScriptsServerMapProperty REUSE_FROM dScriptsBase)
target_include_directories(dScriptsServerMapProperty PUBLIC "."
	"AG_Med"
	"AG_Small"
	"NS_Med")
