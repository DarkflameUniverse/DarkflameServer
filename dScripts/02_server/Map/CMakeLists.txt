set(DSCRIPTS_SOURCES_02_SERVER_MAP)

add_subdirectory(AG)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_MAP_AG})
	set(DSCRIPTS_SOURCES_02_SERVER_MAP ${DSCRIPTS_SOURCES_02_SERVER_MAP} "AG/${file}")
endforeach()

add_subdirectory(AG_Spider_Queen)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_MAP_AG_SPIDER_QUEEN})
	set(DSCRIPTS_SOURCES_02_SERVER_MAP ${DSCRIPTS_SOURCES_02_SERVER_MAP} "AG_Spider_Queen/${file}")
endforeach()

add_subdirectory(AM)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_MAP_AM})
	set(DSCRIPTS_SOURCES_02_SERVER_MAP ${DSCRIPTS_SOURCES_02_SERVER_MAP} "AM/${file}")
endforeach()

add_subdirectory(FV)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_MAP_FV})
	set(DSCRIPTS_SOURCES_02_SERVER_MAP ${DSCRIPTS_SOURCES_02_SERVER_MAP} "FV/${file}")
endforeach()

add_subdirectory(General)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_MAP_GENERAL})
	set(DSCRIPTS_SOURCES_02_SERVER_MAP ${DSCRIPTS_SOURCES_02_SERVER_MAP} "General/${file}")
endforeach()

add_subdirectory(GF)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_MAP_GF})
	set(DSCRIPTS_SOURCES_02_SERVER_MAP ${DSCRIPTS_SOURCES_02_SERVER_MAP} "GF/${file}")
endforeach()

add_subdirectory(njhub)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_MAP_NJHUB})
	set(DSCRIPTS_SOURCES_02_SERVER_MAP ${DSCRIPTS_SOURCES_02_SERVER_MAP} "njhub/${file}")
endforeach()

add_subdirectory(NS)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_MAP_NS})
	set(DSCRIPTS_SOURCES_02_SERVER_MAP ${DSCRIPTS_SOURCES_02_SERVER_MAP} "NS/${file}")
endforeach()

add_subdirectory(NT)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_MAP_NT})
	set(DSCRIPTS_SOURCES_02_SERVER_MAP ${DSCRIPTS_SOURCES_02_SERVER_MAP} "NT/${file}")
endforeach()

add_subdirectory(PR)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_MAP_PR})
	set(DSCRIPTS_SOURCES_02_SERVER_MAP ${DSCRIPTS_SOURCES_02_SERVER_MAP} "PR/${file}")
endforeach()

add_subdirectory(Property)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_MAP_PROPERTY})
	set(DSCRIPTS_SOURCES_02_SERVER_MAP ${DSCRIPTS_SOURCES_02_SERVER_MAP} "Property/${file}")
endforeach()

add_subdirectory(SS)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_MAP_SS})
	set(DSCRIPTS_SOURCES_02_SERVER_MAP ${DSCRIPTS_SOURCES_02_SERVER_MAP} "SS/${file}")
endforeach()

add_subdirectory(VE)

foreach(file ${DSCRIPTS_SOURCES_02_SERVER_MAP_VE})
	set(DSCRIPTS_SOURCES_02_SERVER_MAP ${DSCRIPTS_SOURCES_02_SERVER_MAP} "VE/${file}")
endforeach()

set(DSCRIPTS_SOURCES_02_SERVER_MAP ${DSCRIPTS_SOURCES_02_SERVER_MAP} PARENT_SCOPE)
