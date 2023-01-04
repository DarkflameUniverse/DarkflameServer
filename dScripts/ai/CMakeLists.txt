set(DSCRIPTS_SOURCES_AI)

add_subdirectory(ACT)

foreach(file ${DSCRIPTS_SOURCES_AI_ACT})
	set(DSCRIPTS_SOURCES_AI ${DSCRIPTS_SOURCES_AI} "ACT/${file}")
endforeach()

add_subdirectory(AG)

foreach(file ${DSCRIPTS_SOURCES_AI_AG})
	set(DSCRIPTS_SOURCES_AI ${DSCRIPTS_SOURCES_AI} "AG/${file}")
endforeach()

add_subdirectory(FV)

foreach(file ${DSCRIPTS_SOURCES_AI_FV})
	set(DSCRIPTS_SOURCES_AI ${DSCRIPTS_SOURCES_AI} "FV/${file}")
endforeach()

add_subdirectory(GENERAL)

foreach(file ${DSCRIPTS_SOURCES_AI_GENERAL})
	set(DSCRIPTS_SOURCES_AI ${DSCRIPTS_SOURCES_AI} "GENERAL/${file}")
endforeach()

add_subdirectory(GF)

foreach(file ${DSCRIPTS_SOURCES_AI_GF})
	set(DSCRIPTS_SOURCES_AI ${DSCRIPTS_SOURCES_AI} "GF/${file}")
endforeach()

add_subdirectory(MINIGAME)

foreach(file ${DSCRIPTS_SOURCES_AI_MINIGAME})
	set(DSCRIPTS_SOURCES_AI ${DSCRIPTS_SOURCES_AI} "MINIGAME/${file}")
endforeach()

add_subdirectory(NP)

foreach(file ${DSCRIPTS_SOURCES_AI_NP})
	set(DSCRIPTS_SOURCES_AI ${DSCRIPTS_SOURCES_AI} "NP/${file}")
endforeach()

add_subdirectory(NS)

foreach(file ${DSCRIPTS_SOURCES_AI_NS})
	set(DSCRIPTS_SOURCES_AI ${DSCRIPTS_SOURCES_AI} "NS/${file}")
endforeach()

add_subdirectory(PETS)

foreach(file ${DSCRIPTS_SOURCES_AI_PETS})
	set(DSCRIPTS_SOURCES_AI ${DSCRIPTS_SOURCES_AI} "PETS/${file}")
endforeach()

add_subdirectory(PROPERTY)

foreach(file ${DSCRIPTS_SOURCES_AI_PROPERTY})
	set(DSCRIPTS_SOURCES_AI ${DSCRIPTS_SOURCES_AI} "PROPERTY/${file}")
endforeach()

add_subdirectory(RACING)

foreach(file ${DSCRIPTS_SOURCES_AI_RACING})
	set(DSCRIPTS_SOURCES_AI ${DSCRIPTS_SOURCES_AI} "RACING/${file}")
endforeach()

add_subdirectory(SPEC)

foreach(file ${DSCRIPTS_SOURCES_AI_SPEC})
	set(DSCRIPTS_SOURCES_AI ${DSCRIPTS_SOURCES_AI} "SPEC/${file}")
endforeach()

add_subdirectory(WILD)

foreach(file ${DSCRIPTS_SOURCES_AI_WILD})
	set(DSCRIPTS_SOURCES_AI ${DSCRIPTS_SOURCES_AI} "WILD/${file}")
endforeach()

add_subdirectory(YRK)

foreach(file ${DSCRIPTS_SOURCES_AI_YRK})
	set(DSCRIPTS_SOURCES_AI ${DSCRIPTS_SOURCES_AI} "YRK/${file}")
endforeach()

set(DSCRIPTS_SOURCES_AI ${DSCRIPTS_SOURCES_AI} PARENT_SCOPE)
