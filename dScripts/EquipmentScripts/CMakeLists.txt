set(DSCRIPTS_SOURCES_EQUIPMENTSCRIPTS
	"Sunflower.cpp"
	"AnvilOfArmor.cpp"
	"FountainOfImagination.cpp"
	"CauldronOfLife.cpp"
	"PersonalFortress.cpp"
	"BuccaneerValiantShip.cpp"
	"FireFirstSkillonStartup.cpp"
	"StunImmunity.cpp")

add_library(dScriptsEquipmentScripts OBJECT ${DSCRIPTS_SOURCES_EQUIPMENTSCRIPTS})
target_include_directories(dScriptsEquipmentScripts PUBLIC ".")
target_precompile_headers(dScriptsEquipmentScripts REUSE_FROM dScriptsBase)
