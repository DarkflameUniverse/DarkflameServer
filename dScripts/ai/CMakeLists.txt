set(DSCRIPTS_SOURCES_AI)

add_subdirectory(ACT)
add_subdirectory(AG)
add_subdirectory(FV)
add_subdirectory(GENERAL)
add_subdirectory(GF)
add_subdirectory(MINIGAME)
add_subdirectory(NP)
add_subdirectory(NS)
add_subdirectory(PETS)
add_subdirectory(PROPERTY)
add_subdirectory(RACING)
add_subdirectory(SPEC)
add_subdirectory(WILD)

add_library(dScriptsAI INTERFACE)
target_link_libraries(dScriptsAI INTERFACE
	dScriptsAiAct
	dScriptsAiAG
	dScriptsAiFV
	dScriptsAiGeneral
	dScriptsAiGF
	dScriptsAiMinigame
	dScriptsAiNP
	dScriptsAiNS
	dScriptsAiPets
	dScriptsAiProperty
	dScriptsAiRacing
	dScriptsAiSpec
	dScriptsAiWild
)
